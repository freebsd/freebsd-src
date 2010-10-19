/* Disassembler interface for targets using CGEN. -*- C -*-
   CGEN: Cpu tools GENerator

   THIS FILE IS MACHINE GENERATED WITH CGEN.
   - the resultant file is machine generated, cgen-dis.in isn't

   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005
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
#include "libiberty.h"
#include "m32c-desc.h"
#include "m32c-opc.h"
#include "opintl.h"

/* Default text to print if an instruction isn't recognized.  */
#define UNKNOWN_INSN_MSG _("*unknown*")

static void print_normal
  (CGEN_CPU_DESC, void *, long, unsigned int, bfd_vma, int);
static void print_address
  (CGEN_CPU_DESC, void *, bfd_vma, unsigned int, bfd_vma, int) ATTRIBUTE_UNUSED;
static void print_keyword
  (CGEN_CPU_DESC, void *, CGEN_KEYWORD *, long, unsigned int) ATTRIBUTE_UNUSED;
static void print_insn_normal
  (CGEN_CPU_DESC, void *, const CGEN_INSN *, CGEN_FIELDS *, bfd_vma, int);
static int print_insn
  (CGEN_CPU_DESC, bfd_vma,  disassemble_info *, bfd_byte *, unsigned);
static int default_print_insn
  (CGEN_CPU_DESC, bfd_vma, disassemble_info *) ATTRIBUTE_UNUSED;
static int read_insn
  (CGEN_CPU_DESC, bfd_vma, disassemble_info *, bfd_byte *, int, CGEN_EXTRACT_INFO *,
   unsigned long *);

/* -- disassembler routines inserted here.  */

/* -- dis.c */

#include "elf/m32c.h"
#include "elf-bfd.h"

/* Always print the short insn format suffix as ':<char>'.  */

static void
print_suffix (void * dis_info, char suffix)
{
  disassemble_info *info = dis_info;

  (*info->fprintf_func) (info->stream, ":%c", suffix);
}

static void
print_S (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	 void * dis_info,
	 long value ATTRIBUTE_UNUSED,
	 unsigned int attrs ATTRIBUTE_UNUSED,
	 bfd_vma pc ATTRIBUTE_UNUSED,
	 int length ATTRIBUTE_UNUSED)
{
  print_suffix (dis_info, 's');
}


static void
print_G (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	 void * dis_info,
	 long value ATTRIBUTE_UNUSED,
	 unsigned int attrs ATTRIBUTE_UNUSED,
	 bfd_vma pc ATTRIBUTE_UNUSED,
	 int length ATTRIBUTE_UNUSED)
{
  print_suffix (dis_info, 'g');
}

static void
print_Q (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	 void * dis_info,
	 long value ATTRIBUTE_UNUSED,
	 unsigned int attrs ATTRIBUTE_UNUSED,
	 bfd_vma pc ATTRIBUTE_UNUSED,
	 int length ATTRIBUTE_UNUSED)
{
  print_suffix (dis_info, 'q');
}

static void
print_Z (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	 void * dis_info,
	 long value ATTRIBUTE_UNUSED,
	 unsigned int attrs ATTRIBUTE_UNUSED,
	 bfd_vma pc ATTRIBUTE_UNUSED,
	 int length ATTRIBUTE_UNUSED)
{
  print_suffix (dis_info, 'z');
}

/* Print the empty suffix.  */

static void
print_X (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	 void * dis_info ATTRIBUTE_UNUSED,
	 long value ATTRIBUTE_UNUSED,
	 unsigned int attrs ATTRIBUTE_UNUSED,
	 bfd_vma pc ATTRIBUTE_UNUSED,
	 int length ATTRIBUTE_UNUSED)
{
  return;
}

static void
print_r0l_r0h (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	       void * dis_info,
	       long value,
	       unsigned int attrs ATTRIBUTE_UNUSED,
	       bfd_vma pc ATTRIBUTE_UNUSED,
	       int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = dis_info;

  if (value == 0)
    (*info->fprintf_func) (info->stream, "r0h,r0l");
  else
    (*info->fprintf_func) (info->stream, "r0l,r0h");
}

static void
print_unsigned_bitbase (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
			void * dis_info,
			unsigned long value,
			unsigned int attrs ATTRIBUTE_UNUSED,
			bfd_vma pc ATTRIBUTE_UNUSED,
			int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = dis_info;

  (*info->fprintf_func) (info->stream, "%ld,0x%lx", value & 0x7, value >> 3);
}

static void
print_signed_bitbase (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		      void * dis_info,
		      signed long value,
		      unsigned int attrs ATTRIBUTE_UNUSED,
		      bfd_vma pc ATTRIBUTE_UNUSED,
		      int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = dis_info;

  (*info->fprintf_func) (info->stream, "%ld,%ld", value & 0x7, value >> 3);
}

static void
print_size (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	    void * dis_info,
	    long value ATTRIBUTE_UNUSED,
	    unsigned int attrs ATTRIBUTE_UNUSED,
	    bfd_vma pc ATTRIBUTE_UNUSED,
	    int length ATTRIBUTE_UNUSED)
{
  /* Always print the size as '.w'.  */
  disassemble_info *info = dis_info;

  (*info->fprintf_func) (info->stream, ".w");
}

#define POP  0
#define PUSH 1

static void print_pop_regset  (CGEN_CPU_DESC, void *, long, unsigned int, bfd_vma, int);
static void print_push_regset (CGEN_CPU_DESC, void *, long, unsigned int, bfd_vma, int);

/* Print a set of registers, R0,R1,A0,A1,SB,FB.  */

static void
print_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	      void * dis_info,
	      long value,
	      unsigned int attrs ATTRIBUTE_UNUSED,
	      bfd_vma pc ATTRIBUTE_UNUSED,
	      int length ATTRIBUTE_UNUSED,
	      int push)
{
  static char * m16c_register_names [] = 
  {
    "r0", "r1", "r2", "r3", "a0", "a1", "sb", "fb"
  };
  disassemble_info *info = dis_info;
  int mask;
  int index = 0;
  char* comma = "";

  if (push)
    mask = 0x80;
  else
    mask = 1;
 
  if (value & mask)
    {
      (*info->fprintf_func) (info->stream, "%s", m16c_register_names [0]);
      comma = ",";
    }

  for (index = 1; index <= 7; ++index)
    {
      if (push)
        mask >>= 1;
      else
        mask <<= 1;

      if (value & mask)
        {
          (*info->fprintf_func) (info->stream, "%s%s", comma,
				 m16c_register_names [index]);
          comma = ",";
        }
    }
}

static void
print_pop_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		  void * dis_info,
		  long value,
		  unsigned int attrs ATTRIBUTE_UNUSED,
		  bfd_vma pc ATTRIBUTE_UNUSED,
		  int length ATTRIBUTE_UNUSED)
{
  print_regset (cd, dis_info, value, attrs, pc, length, POP);
}

static void
print_push_regset (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		   void * dis_info,
		   long value,
		   unsigned int attrs ATTRIBUTE_UNUSED,
		   bfd_vma pc ATTRIBUTE_UNUSED,
		   int length ATTRIBUTE_UNUSED)
{
  print_regset (cd, dis_info, value, attrs, pc, length, PUSH);
}

static void
print_signed4n (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
		void * dis_info,
		signed long value,
		unsigned int attrs ATTRIBUTE_UNUSED,
		bfd_vma pc ATTRIBUTE_UNUSED,
		int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = dis_info;

  (*info->fprintf_func) (info->stream, "%ld", -value);
}

void m32c_cgen_print_operand
  (CGEN_CPU_DESC, int, PTR, CGEN_FIELDS *, void const *, bfd_vma, int);

/* Main entry point for printing operands.
   XINFO is a `void *' and not a `disassemble_info *' to not put a requirement
   of dis-asm.h on cgen.h.

   This function is basically just a big switch statement.  Earlier versions
   used tables to look up the function to use, but
   - if the table contains both assembler and disassembler functions then
     the disassembler contains much of the assembler and vice-versa,
   - there's a lot of inlining possibilities as things grow,
   - using a switch statement avoids the function call overhead.

   This function could be moved into `print_insn_normal', but keeping it
   separate makes clear the interface between `print_insn_normal' and each of
   the handlers.  */

void
m32c_cgen_print_operand (CGEN_CPU_DESC cd,
			   int opindex,
			   void * xinfo,
			   CGEN_FIELDS *fields,
			   void const *attrs ATTRIBUTE_UNUSED,
			   bfd_vma pc,
			   int length)
{
  disassemble_info *info = (disassemble_info *) xinfo;

  switch (opindex)
    {
    case M32C_OPERAND_A0 :
      print_keyword (cd, info, & m32c_cgen_opval_h_a0, 0, 0);
      break;
    case M32C_OPERAND_A1 :
      print_keyword (cd, info, & m32c_cgen_opval_h_a1, 0, 0);
      break;
    case M32C_OPERAND_AN16_PUSH_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_4_1, 0);
      break;
    case M32C_OPERAND_BIT16AN :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst16_an, 0);
      break;
    case M32C_OPERAND_BIT16RN :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_dst16_rn, 0);
      break;
    case M32C_OPERAND_BIT3_S :
      print_normal (cd, info, fields->f_imm3_S, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BIT32ANPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_prefixed, 0);
      break;
    case M32C_OPERAND_BIT32ANUNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_BIT32RNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_dst32_rn_prefixed_QI, 0);
      break;
    case M32C_OPERAND_BIT32RNUNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_dst32_rn_unprefixed_QI, 0);
      break;
    case M32C_OPERAND_BITBASE16_16_S8 :
      print_signed_bitbase (cd, info, fields->f_dsp_16_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_BITBASE16_16_U16 :
      print_unsigned_bitbase (cd, info, fields->f_dsp_16_u16, 0, pc, length);
      break;
    case M32C_OPERAND_BITBASE16_16_U8 :
      print_unsigned_bitbase (cd, info, fields->f_dsp_16_u8, 0, pc, length);
      break;
    case M32C_OPERAND_BITBASE16_8_U11_S :
      print_unsigned_bitbase (cd, info, fields->f_bitbase16_u11_S, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED :
      print_signed_bitbase (cd, info, fields->f_bitbase32_16_s11_unprefixed, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED :
      print_signed_bitbase (cd, info, fields->f_bitbase32_16_s19_unprefixed, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_16_u11_unprefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_16_u19_unprefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_16_u27_unprefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_24_S11_PREFIXED :
      print_signed_bitbase (cd, info, fields->f_bitbase32_24_s11_prefixed, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_24_S19_PREFIXED :
      print_signed_bitbase (cd, info, fields->f_bitbase32_24_s19_prefixed, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_24_U11_PREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_24_u11_prefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_24_U19_PREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_24_u19_prefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITBASE32_24_U27_PREFIXED :
      print_unsigned_bitbase (cd, info, fields->f_bitbase32_24_u27_prefixed, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_BITNO16R :
      print_normal (cd, info, fields->f_dsp_16_u8, 0, pc, length);
      break;
    case M32C_OPERAND_BITNO32PREFIXED :
      print_normal (cd, info, fields->f_bitno32_prefixed, 0, pc, length);
      break;
    case M32C_OPERAND_BITNO32UNPREFIXED :
      print_normal (cd, info, fields->f_bitno32_unprefixed, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_10_U6 :
      print_normal (cd, info, fields->f_dsp_10_u6, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_16_S16 :
      print_normal (cd, info, fields->f_dsp_16_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_16_S8 :
      print_normal (cd, info, fields->f_dsp_16_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_16_U16 :
      print_normal (cd, info, fields->f_dsp_16_u16, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_16_U20 :
      print_normal (cd, info, fields->f_dsp_16_u24, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_16_U24 :
      print_normal (cd, info, fields->f_dsp_16_u24, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_16_U8 :
      print_normal (cd, info, fields->f_dsp_16_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_24_S16 :
      print_normal (cd, info, fields->f_dsp_24_s16, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_24_S8 :
      print_normal (cd, info, fields->f_dsp_24_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_24_U16 :
      print_normal (cd, info, fields->f_dsp_24_u16, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_24_U20 :
      print_normal (cd, info, fields->f_dsp_24_u24, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_24_U24 :
      print_normal (cd, info, fields->f_dsp_24_u24, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_24_U8 :
      print_normal (cd, info, fields->f_dsp_24_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_32_S16 :
      print_normal (cd, info, fields->f_dsp_32_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_32_S8 :
      print_normal (cd, info, fields->f_dsp_32_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_32_U16 :
      print_normal (cd, info, fields->f_dsp_32_u16, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_32_U20 :
      print_normal (cd, info, fields->f_dsp_32_u24, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_32_U24 :
      print_normal (cd, info, fields->f_dsp_32_u24, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_32_U8 :
      print_normal (cd, info, fields->f_dsp_32_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_40_S16 :
      print_normal (cd, info, fields->f_dsp_40_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_40_S8 :
      print_normal (cd, info, fields->f_dsp_40_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_40_U16 :
      print_normal (cd, info, fields->f_dsp_40_u16, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_40_U24 :
      print_normal (cd, info, fields->f_dsp_40_u24, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_40_U8 :
      print_normal (cd, info, fields->f_dsp_40_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_48_S16 :
      print_normal (cd, info, fields->f_dsp_48_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_48_S8 :
      print_normal (cd, info, fields->f_dsp_48_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_48_U16 :
      print_normal (cd, info, fields->f_dsp_48_u16, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_48_U24 :
      print_normal (cd, info, fields->f_dsp_48_u24, 0|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_DSP_48_U8 :
      print_normal (cd, info, fields->f_dsp_48_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_8_S24 :
      print_normal (cd, info, fields->f_dsp_8_s24, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_8_S8 :
      print_normal (cd, info, fields->f_dsp_8_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_DSP_8_U16 :
      print_normal (cd, info, fields->f_dsp_8_u16, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_8_U24 :
      print_normal (cd, info, fields->f_dsp_8_u24, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_8_U6 :
      print_normal (cd, info, fields->f_dsp_8_u6, 0, pc, length);
      break;
    case M32C_OPERAND_DSP_8_U8 :
      print_normal (cd, info, fields->f_dsp_8_u8, 0, pc, length);
      break;
    case M32C_OPERAND_DST16AN :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst16_an, 0);
      break;
    case M32C_OPERAND_DST16AN_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_dst16_an_s, 0);
      break;
    case M32C_OPERAND_DST16ANHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_dst16_an, 0);
      break;
    case M32C_OPERAND_DST16ANQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_dst16_an, 0);
      break;
    case M32C_OPERAND_DST16ANQI_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_dst16_rn_QI_s, 0);
      break;
    case M32C_OPERAND_DST16ANSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_SI, fields->f_dst16_an, 0);
      break;
    case M32C_OPERAND_DST16RNEXTQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_ext_QI, fields->f_dst16_rn_ext, 0);
      break;
    case M32C_OPERAND_DST16RNHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_dst16_rn, 0);
      break;
    case M32C_OPERAND_DST16RNQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_dst16_rn, 0);
      break;
    case M32C_OPERAND_DST16RNQI_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0l_r0h, fields->f_dst16_rn_QI_s, 0);
      break;
    case M32C_OPERAND_DST16RNSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_SI, fields->f_dst16_rn, 0);
      break;
    case M32C_OPERAND_DST32ANEXTUNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32ANPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_prefixed, 0);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_dst32_an_prefixed, 0);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_dst32_an_prefixed, 0);
      break;
    case M32C_OPERAND_DST32ANPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_prefixed, 0);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32ANUNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_dst32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32R0HI_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0, 0, 0);
      break;
    case M32C_OPERAND_DST32R0QI_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0l, 0, 0);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_ext_HI, fields->f_dst32_rn_ext_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32RNEXTUNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_ext_QI, fields->f_dst32_rn_ext_unprefixed, 0);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_dst32_rn_prefixed_HI, 0);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_dst32_rn_prefixed_QI, 0);
      break;
    case M32C_OPERAND_DST32RNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_SI, fields->f_dst32_rn_prefixed_SI, 0);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_dst32_rn_unprefixed_HI, 0);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_dst32_rn_unprefixed_QI, 0);
      break;
    case M32C_OPERAND_DST32RNUNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_SI, fields->f_dst32_rn_unprefixed_SI, 0);
      break;
    case M32C_OPERAND_G :
      print_G (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_12_S4 :
      print_normal (cd, info, fields->f_imm_12_s4, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_12_S4N :
      print_signed4n (cd, info, fields->f_imm_12_s4, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_13_U3 :
      print_normal (cd, info, fields->f_imm_13_u3, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_16_HI :
      print_normal (cd, info, fields->f_dsp_16_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_16_QI :
      print_normal (cd, info, fields->f_dsp_16_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_16_SI :
      print_normal (cd, info, fields->f_dsp_16_s32, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_20_S4 :
      print_normal (cd, info, fields->f_imm_20_s4, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_24_HI :
      print_normal (cd, info, fields->f_dsp_24_s16, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_24_QI :
      print_normal (cd, info, fields->f_dsp_24_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_24_SI :
      print_normal (cd, info, fields->f_dsp_24_s32, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_32_HI :
      print_normal (cd, info, fields->f_dsp_32_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_32_QI :
      print_normal (cd, info, fields->f_dsp_32_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_32_SI :
      print_normal (cd, info, fields->f_dsp_32_s32, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_40_HI :
      print_normal (cd, info, fields->f_dsp_40_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_40_QI :
      print_normal (cd, info, fields->f_dsp_40_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_40_SI :
      print_normal (cd, info, fields->f_dsp_40_s32, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_48_HI :
      print_normal (cd, info, fields->f_dsp_48_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_48_QI :
      print_normal (cd, info, fields->f_dsp_48_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_48_SI :
      print_normal (cd, info, fields->f_dsp_48_s32, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_56_HI :
      print_normal (cd, info, fields->f_dsp_56_s16, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_IMM_56_QI :
      print_normal (cd, info, fields->f_dsp_56_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_64_HI :
      print_normal (cd, info, fields->f_dsp_64_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_8_HI :
      print_normal (cd, info, fields->f_dsp_8_s16, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_8_QI :
      print_normal (cd, info, fields->f_dsp_8_s8, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_8_S4 :
      print_normal (cd, info, fields->f_imm_8_s4, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_8_S4N :
      print_normal (cd, info, fields->f_imm_8_s4, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM_SH_12_S4 :
      print_keyword (cd, info, & m32c_cgen_opval_h_shimm, fields->f_imm_12_s4, 0);
      break;
    case M32C_OPERAND_IMM_SH_20_S4 :
      print_keyword (cd, info, & m32c_cgen_opval_h_shimm, fields->f_imm_20_s4, 0);
      break;
    case M32C_OPERAND_IMM_SH_8_S4 :
      print_keyword (cd, info, & m32c_cgen_opval_h_shimm, fields->f_imm_8_s4, 0);
      break;
    case M32C_OPERAND_IMM1_S :
      print_normal (cd, info, fields->f_imm1_S, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_IMM3_S :
      print_normal (cd, info, fields->f_imm3_S, 0|(1<<CGEN_OPERAND_SIGNED)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_LAB_16_8 :
      print_address (cd, info, fields->f_lab_16_8, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_24_8 :
      print_address (cd, info, fields->f_lab_24_8, 0|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_32_8 :
      print_address (cd, info, fields->f_lab_32_8, 0|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_40_8 :
      print_address (cd, info, fields->f_lab_40_8, 0|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_5_3 :
      print_address (cd, info, fields->f_lab_5_3, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_8_16 :
      print_address (cd, info, fields->f_lab_8_16, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_SIGN_OPT)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_8_24 :
      print_address (cd, info, fields->f_lab_8_24, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_ABS_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB_8_8 :
      print_address (cd, info, fields->f_lab_8_8, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_PCREL_ADDR), pc, length);
      break;
    case M32C_OPERAND_LAB32_JMP_S :
      print_address (cd, info, fields->f_lab32_jmp_s, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_PCREL_ADDR)|(1<<CGEN_OPERAND_VIRTUAL), pc, length);
      break;
    case M32C_OPERAND_Q :
      print_Q (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_R0 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0, 0, 0);
      break;
    case M32C_OPERAND_R0H :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0h, 0, 0);
      break;
    case M32C_OPERAND_R0L :
      print_keyword (cd, info, & m32c_cgen_opval_h_r0l, 0, 0);
      break;
    case M32C_OPERAND_R1 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r1, 0, 0);
      break;
    case M32C_OPERAND_R1R2R0 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r1r2r0, 0, 0);
      break;
    case M32C_OPERAND_R2 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r2, 0, 0);
      break;
    case M32C_OPERAND_R2R0 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r2r0, 0, 0);
      break;
    case M32C_OPERAND_R3 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r3, 0, 0);
      break;
    case M32C_OPERAND_R3R1 :
      print_keyword (cd, info, & m32c_cgen_opval_h_r3r1, 0, 0);
      break;
    case M32C_OPERAND_REGSETPOP :
      print_pop_regset (cd, info, fields->f_8_8, 0, pc, length);
      break;
    case M32C_OPERAND_REGSETPUSH :
      print_push_regset (cd, info, fields->f_8_8, 0, pc, length);
      break;
    case M32C_OPERAND_RN16_PUSH_S :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_4_1, 0);
      break;
    case M32C_OPERAND_S :
      print_S (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_SRC16AN :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_src16_an, 0);
      break;
    case M32C_OPERAND_SRC16ANHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_src16_an, 0);
      break;
    case M32C_OPERAND_SRC16ANQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_src16_an, 0);
      break;
    case M32C_OPERAND_SRC16RNHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_src16_rn, 0);
      break;
    case M32C_OPERAND_SRC16RNQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_src16_rn, 0);
      break;
    case M32C_OPERAND_SRC32ANPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_src32_an_prefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_src32_an_prefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_src32_an_prefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_src32_an_prefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXED :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_src32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_HI, fields->f_src32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar_QI, fields->f_src32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_SRC32ANUNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_ar, fields->f_src32_an_unprefixed, 0);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_src32_rn_prefixed_HI, 0);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_src32_rn_prefixed_QI, 0);
      break;
    case M32C_OPERAND_SRC32RNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_SI, fields->f_src32_rn_prefixed_SI, 0);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDHI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_HI, fields->f_src32_rn_unprefixed_HI, 0);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDQI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_QI, fields->f_src32_rn_unprefixed_QI, 0);
      break;
    case M32C_OPERAND_SRC32RNUNPREFIXEDSI :
      print_keyword (cd, info, & m32c_cgen_opval_h_gr_SI, fields->f_src32_rn_unprefixed_SI, 0);
      break;
    case M32C_OPERAND_SRCDST16_R0L_R0H_S_NORMAL :
      print_r0l_r0h (cd, info, fields->f_5_1, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_X :
      print_X (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_Z :
      print_Z (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;
    case M32C_OPERAND_COND16_16 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16, fields->f_dsp_16_u8, 0);
      break;
    case M32C_OPERAND_COND16_24 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16, fields->f_dsp_24_u8, 0);
      break;
    case M32C_OPERAND_COND16_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16, fields->f_dsp_32_u8, 0);
      break;
    case M32C_OPERAND_COND16C :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16c, fields->f_cond16, 0);
      break;
    case M32C_OPERAND_COND16J :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16j, fields->f_cond16, 0);
      break;
    case M32C_OPERAND_COND16J5 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond16j_5, fields->f_cond16j_5, 0);
      break;
    case M32C_OPERAND_COND32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_cond32, 0|(1<<CGEN_OPERAND_VIRTUAL));
      break;
    case M32C_OPERAND_COND32_16 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_dsp_16_u8, 0);
      break;
    case M32C_OPERAND_COND32_24 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_dsp_24_u8, 0);
      break;
    case M32C_OPERAND_COND32_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_dsp_32_u8, 0);
      break;
    case M32C_OPERAND_COND32_40 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_dsp_40_u8, 0);
      break;
    case M32C_OPERAND_COND32J :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_cond32j, 0|(1<<CGEN_OPERAND_VIRTUAL));
      break;
    case M32C_OPERAND_CR1_PREFIXED_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr1_32, fields->f_21_3, 0);
      break;
    case M32C_OPERAND_CR1_UNPREFIXED_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr1_32, fields->f_13_3, 0);
      break;
    case M32C_OPERAND_CR16 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr_16, fields->f_9_3, 0);
      break;
    case M32C_OPERAND_CR2_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr2_32, fields->f_13_3, 0);
      break;
    case M32C_OPERAND_CR3_PREFIXED_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr3_32, fields->f_21_3, 0);
      break;
    case M32C_OPERAND_CR3_UNPREFIXED_32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cr3_32, fields->f_13_3, 0);
      break;
    case M32C_OPERAND_FLAGS16 :
      print_keyword (cd, info, & m32c_cgen_opval_h_flags, fields->f_9_3, 0);
      break;
    case M32C_OPERAND_FLAGS32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_flags, fields->f_13_3, 0);
      break;
    case M32C_OPERAND_SCCOND32 :
      print_keyword (cd, info, & m32c_cgen_opval_h_cond32, fields->f_cond16, 0);
      break;
    case M32C_OPERAND_SIZE :
      print_size (cd, info, 0, 0|(1<<CGEN_OPERAND_SIGNED), pc, length);
      break;

    default :
      /* xgettext:c-format */
      fprintf (stderr, _("Unrecognized field %d while printing insn.\n"),
	       opindex);
    abort ();
  }
}

cgen_print_fn * const m32c_cgen_print_handlers[] = 
{
  print_insn_normal,
};


void
m32c_cgen_init_dis (CGEN_CPU_DESC cd)
{
  m32c_cgen_init_opcode_table (cd);
  m32c_cgen_init_ibld_table (cd);
  cd->print_handlers = & m32c_cgen_print_handlers[0];
  cd->print_operand = m32c_cgen_print_operand;
}


/* Default print handler.  */

static void
print_normal (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	      void *dis_info,
	      long value,
	      unsigned int attrs,
	      bfd_vma pc ATTRIBUTE_UNUSED,
	      int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = (disassemble_info *) dis_info;

#ifdef CGEN_PRINT_NORMAL
  CGEN_PRINT_NORMAL (cd, info, value, attrs, pc, length);
#endif

  /* Print the operand as directed by the attributes.  */
  if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_SEM_ONLY))
    ; /* nothing to do */
  else if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_SIGNED))
    (*info->fprintf_func) (info->stream, "%ld", value);
  else
    (*info->fprintf_func) (info->stream, "0x%lx", value);
}

/* Default address handler.  */

static void
print_address (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	       void *dis_info,
	       bfd_vma value,
	       unsigned int attrs,
	       bfd_vma pc ATTRIBUTE_UNUSED,
	       int length ATTRIBUTE_UNUSED)
{
  disassemble_info *info = (disassemble_info *) dis_info;

#ifdef CGEN_PRINT_ADDRESS
  CGEN_PRINT_ADDRESS (cd, info, value, attrs, pc, length);
#endif

  /* Print the operand as directed by the attributes.  */
  if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_SEM_ONLY))
    ; /* Nothing to do.  */
  else if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_PCREL_ADDR))
    (*info->print_address_func) (value, info);
  else if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_ABS_ADDR))
    (*info->print_address_func) (value, info);
  else if (CGEN_BOOL_ATTR (attrs, CGEN_OPERAND_SIGNED))
    (*info->fprintf_func) (info->stream, "%ld", (long) value);
  else
    (*info->fprintf_func) (info->stream, "0x%lx", (long) value);
}

/* Keyword print handler.  */

static void
print_keyword (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	       void *dis_info,
	       CGEN_KEYWORD *keyword_table,
	       long value,
	       unsigned int attrs ATTRIBUTE_UNUSED)
{
  disassemble_info *info = (disassemble_info *) dis_info;
  const CGEN_KEYWORD_ENTRY *ke;

  ke = cgen_keyword_lookup_value (keyword_table, value);
  if (ke != NULL)
    (*info->fprintf_func) (info->stream, "%s", ke->name);
  else
    (*info->fprintf_func) (info->stream, "???");
}

/* Default insn printer.

   DIS_INFO is defined as `void *' so the disassembler needn't know anything
   about disassemble_info.  */

static void
print_insn_normal (CGEN_CPU_DESC cd,
		   void *dis_info,
		   const CGEN_INSN *insn,
		   CGEN_FIELDS *fields,
		   bfd_vma pc,
		   int length)
{
  const CGEN_SYNTAX *syntax = CGEN_INSN_SYNTAX (insn);
  disassemble_info *info = (disassemble_info *) dis_info;
  const CGEN_SYNTAX_CHAR_TYPE *syn;

  CGEN_INIT_PRINT (cd);

  for (syn = CGEN_SYNTAX_STRING (syntax); *syn; ++syn)
    {
      if (CGEN_SYNTAX_MNEMONIC_P (*syn))
	{
	  (*info->fprintf_func) (info->stream, "%s", CGEN_INSN_MNEMONIC (insn));
	  continue;
	}
      if (CGEN_SYNTAX_CHAR_P (*syn))
	{
	  (*info->fprintf_func) (info->stream, "%c", CGEN_SYNTAX_CHAR (*syn));
	  continue;
	}

      /* We have an operand.  */
      m32c_cgen_print_operand (cd, CGEN_SYNTAX_FIELD (*syn), info,
				 fields, CGEN_INSN_ATTRS (insn), pc, length);
    }
}

/* Subroutine of print_insn. Reads an insn into the given buffers and updates
   the extract info.
   Returns 0 if all is well, non-zero otherwise.  */

static int
read_insn (CGEN_CPU_DESC cd ATTRIBUTE_UNUSED,
	   bfd_vma pc,
	   disassemble_info *info,
	   bfd_byte *buf,
	   int buflen,
	   CGEN_EXTRACT_INFO *ex_info,
	   unsigned long *insn_value)
{
  int status = (*info->read_memory_func) (pc, buf, buflen, info);

  if (status != 0)
    {
      (*info->memory_error_func) (status, pc, info);
      return -1;
    }

  ex_info->dis_info = info;
  ex_info->valid = (1 << buflen) - 1;
  ex_info->insn_bytes = buf;

  *insn_value = bfd_get_bits (buf, buflen * 8, info->endian == BFD_ENDIAN_BIG);
  return 0;
}

/* Utility to print an insn.
   BUF is the base part of the insn, target byte order, BUFLEN bytes long.
   The result is the size of the insn in bytes or zero for an unknown insn
   or -1 if an error occurs fetching data (memory_error_func will have
   been called).  */

static int
print_insn (CGEN_CPU_DESC cd,
	    bfd_vma pc,
	    disassemble_info *info,
	    bfd_byte *buf,
	    unsigned int buflen)
{
  CGEN_INSN_INT insn_value;
  const CGEN_INSN_LIST *insn_list;
  CGEN_EXTRACT_INFO ex_info;
  int basesize;

  /* Extract base part of instruction, just in case CGEN_DIS_* uses it. */
  basesize = cd->base_insn_bitsize < buflen * 8 ?
                                     cd->base_insn_bitsize : buflen * 8;
  insn_value = cgen_get_insn_value (cd, buf, basesize);


  /* Fill in ex_info fields like read_insn would.  Don't actually call
     read_insn, since the incoming buffer is already read (and possibly
     modified a la m32r).  */
  ex_info.valid = (1 << buflen) - 1;
  ex_info.dis_info = info;
  ex_info.insn_bytes = buf;

  /* The instructions are stored in hash lists.
     Pick the first one and keep trying until we find the right one.  */

  insn_list = CGEN_DIS_LOOKUP_INSN (cd, (char *) buf, insn_value);
  while (insn_list != NULL)
    {
      const CGEN_INSN *insn = insn_list->insn;
      CGEN_FIELDS fields;
      int length;
      unsigned long insn_value_cropped;

#ifdef CGEN_VALIDATE_INSN_SUPPORTED 
      /* Not needed as insn shouldn't be in hash lists if not supported.  */
      /* Supported by this cpu?  */
      if (! m32c_cgen_insn_supported (cd, insn))
        {
          insn_list = CGEN_DIS_NEXT_INSN (insn_list);
	  continue;
        }
#endif

      /* Basic bit mask must be correct.  */
      /* ??? May wish to allow target to defer this check until the extract
	 handler.  */

      /* Base size may exceed this instruction's size.  Extract the
         relevant part from the buffer. */
      if ((unsigned) (CGEN_INSN_BITSIZE (insn) / 8) < buflen &&
	  (unsigned) (CGEN_INSN_BITSIZE (insn) / 8) <= sizeof (unsigned long))
	insn_value_cropped = bfd_get_bits (buf, CGEN_INSN_BITSIZE (insn), 
					   info->endian == BFD_ENDIAN_BIG);
      else
	insn_value_cropped = insn_value;

      if ((insn_value_cropped & CGEN_INSN_BASE_MASK (insn))
	  == CGEN_INSN_BASE_VALUE (insn))
	{
	  /* Printing is handled in two passes.  The first pass parses the
	     machine insn and extracts the fields.  The second pass prints
	     them.  */

	  /* Make sure the entire insn is loaded into insn_value, if it
	     can fit.  */
	  if (((unsigned) CGEN_INSN_BITSIZE (insn) > cd->base_insn_bitsize) &&
	      (unsigned) (CGEN_INSN_BITSIZE (insn) / 8) <= sizeof (unsigned long))
	    {
	      unsigned long full_insn_value;
	      int rc = read_insn (cd, pc, info, buf,
				  CGEN_INSN_BITSIZE (insn) / 8,
				  & ex_info, & full_insn_value);
	      if (rc != 0)
		return rc;
	      length = CGEN_EXTRACT_FN (cd, insn)
		(cd, insn, &ex_info, full_insn_value, &fields, pc);
	    }
	  else
	    length = CGEN_EXTRACT_FN (cd, insn)
	      (cd, insn, &ex_info, insn_value_cropped, &fields, pc);

	  /* Length < 0 -> error.  */
	  if (length < 0)
	    return length;
	  if (length > 0)
	    {
	      CGEN_PRINT_FN (cd, insn) (cd, info, insn, &fields, pc, length);
	      /* Length is in bits, result is in bytes.  */
	      return length / 8;
	    }
	}

      insn_list = CGEN_DIS_NEXT_INSN (insn_list);
    }

  return 0;
}

/* Default value for CGEN_PRINT_INSN.
   The result is the size of the insn in bytes or zero for an unknown insn
   or -1 if an error occured fetching bytes.  */

#ifndef CGEN_PRINT_INSN
#define CGEN_PRINT_INSN default_print_insn
#endif

static int
default_print_insn (CGEN_CPU_DESC cd, bfd_vma pc, disassemble_info *info)
{
  bfd_byte buf[CGEN_MAX_INSN_SIZE];
  int buflen;
  int status;

  /* Attempt to read the base part of the insn.  */
  buflen = cd->base_insn_bitsize / 8;
  status = (*info->read_memory_func) (pc, buf, buflen, info);

  /* Try again with the minimum part, if min < base.  */
  if (status != 0 && (cd->min_insn_bitsize < cd->base_insn_bitsize))
    {
      buflen = cd->min_insn_bitsize / 8;
      status = (*info->read_memory_func) (pc, buf, buflen, info);
    }

  if (status != 0)
    {
      (*info->memory_error_func) (status, pc, info);
      return -1;
    }

  return print_insn (cd, pc, info, buf, buflen);
}

/* Main entry point.
   Print one instruction from PC on INFO->STREAM.
   Return the size of the instruction (in bytes).  */

typedef struct cpu_desc_list
{
  struct cpu_desc_list *next;
  CGEN_BITSET *isa;
  int mach;
  int endian;
  CGEN_CPU_DESC cd;
} cpu_desc_list;

int
print_insn_m32c (bfd_vma pc, disassemble_info *info)
{
  static cpu_desc_list *cd_list = 0;
  cpu_desc_list *cl = 0;
  static CGEN_CPU_DESC cd = 0;
  static CGEN_BITSET *prev_isa;
  static int prev_mach;
  static int prev_endian;
  int length;
  CGEN_BITSET *isa;
  int mach;
  int endian = (info->endian == BFD_ENDIAN_BIG
		? CGEN_ENDIAN_BIG
		: CGEN_ENDIAN_LITTLE);
  enum bfd_architecture arch;

  /* ??? gdb will set mach but leave the architecture as "unknown" */
#ifndef CGEN_BFD_ARCH
#define CGEN_BFD_ARCH bfd_arch_m32c
#endif
  arch = info->arch;
  if (arch == bfd_arch_unknown)
    arch = CGEN_BFD_ARCH;
   
  /* There's no standard way to compute the machine or isa number
     so we leave it to the target.  */
#ifdef CGEN_COMPUTE_MACH
  mach = CGEN_COMPUTE_MACH (info);
#else
  mach = info->mach;
#endif

#ifdef CGEN_COMPUTE_ISA
  {
    static CGEN_BITSET *permanent_isa;

    if (!permanent_isa)
      permanent_isa = cgen_bitset_create (MAX_ISAS);
    isa = permanent_isa;
    cgen_bitset_clear (isa);
    cgen_bitset_add (isa, CGEN_COMPUTE_ISA (info));
  }
#else
  isa = info->insn_sets;
#endif

  /* If we've switched cpu's, try to find a handle we've used before */
  if (cd
      && (cgen_bitset_compare (isa, prev_isa) != 0
	  || mach != prev_mach
	  || endian != prev_endian))
    {
      cd = 0;
      for (cl = cd_list; cl; cl = cl->next)
	{
	  if (cgen_bitset_compare (cl->isa, isa) == 0 &&
	      cl->mach == mach &&
	      cl->endian == endian)
	    {
	      cd = cl->cd;
 	      prev_isa = cd->isas;
	      break;
	    }
	}
    } 

  /* If we haven't initialized yet, initialize the opcode table.  */
  if (! cd)
    {
      const bfd_arch_info_type *arch_type = bfd_lookup_arch (arch, mach);
      const char *mach_name;

      if (!arch_type)
	abort ();
      mach_name = arch_type->printable_name;

      prev_isa = cgen_bitset_copy (isa);
      prev_mach = mach;
      prev_endian = endian;
      cd = m32c_cgen_cpu_open (CGEN_CPU_OPEN_ISAS, prev_isa,
				 CGEN_CPU_OPEN_BFDMACH, mach_name,
				 CGEN_CPU_OPEN_ENDIAN, prev_endian,
				 CGEN_CPU_OPEN_END);
      if (!cd)
	abort ();

      /* Save this away for future reference.  */
      cl = xmalloc (sizeof (struct cpu_desc_list));
      cl->cd = cd;
      cl->isa = prev_isa;
      cl->mach = mach;
      cl->endian = endian;
      cl->next = cd_list;
      cd_list = cl;

      m32c_cgen_init_dis (cd);
    }

  /* We try to have as much common code as possible.
     But at this point some targets need to take over.  */
  /* ??? Some targets may need a hook elsewhere.  Try to avoid this,
     but if not possible try to move this hook elsewhere rather than
     have two hooks.  */
  length = CGEN_PRINT_INSN (cd, pc, info);
  if (length > 0)
    return length;
  if (length < 0)
    return -1;

  (*info->fprintf_func) (info->stream, UNKNOWN_INSN_MSG);
  return cd->default_insn_bitsize / 8;
}
