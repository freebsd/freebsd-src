/* HP PA-RISC SOM object file format:  definitions internal to BFD.
   Copyright (C) 1990, 91, 92, 93, 94 Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _HPPA_H
#define _HPPA_H

#define BYTES_IN_WORD 4
#define PA_PAGESIZE 0x1000

#ifndef INLINE
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif /* GNU C? */
#endif /* INLINE */

/* The PA instruction set variants.  */
enum pa_arch {pa10 = 10, pa11 = 11, pa20 = 20};

/* HP PA-RISC relocation types */

enum hppa_reloc_field_selector_type
  {
    R_HPPA_FSEL = 0x0,
    R_HPPA_LSSEL = 0x1,
    R_HPPA_RSSEL = 0x2,
    R_HPPA_LSEL = 0x3,
    R_HPPA_RSEL = 0x4,
    R_HPPA_LDSEL = 0x5,
    R_HPPA_RDSEL = 0x6,
    R_HPPA_LRSEL = 0x7,
    R_HPPA_RRSEL = 0x8,
    R_HPPA_NSEL  = 0x9,
    R_HPPA_NLSEL  = 0xa,
    R_HPPA_NLRSEL  = 0xb,
    R_HPPA_PSEL = 0xc,
    R_HPPA_LPSEL = 0xd,
    R_HPPA_RPSEL = 0xe,
    R_HPPA_TSEL = 0xf,
    R_HPPA_LTSEL = 0x10,
    R_HPPA_RTSEL = 0x11
  };

/* /usr/include/reloc.h defines these to constants.  We want to use
   them in enums, so #undef them before we start using them.  We might
   be able to fix this another way by simply managing not to include
   /usr/include/reloc.h, but currently GDB picks up these defines
   somewhere.  */
#undef e_fsel
#undef e_lssel
#undef e_rssel
#undef e_lsel
#undef e_rsel
#undef e_ldsel
#undef e_rdsel
#undef e_lrsel
#undef e_rrsel
#undef e_nsel
#undef e_nlsel
#undef e_nlrsel
#undef e_psel
#undef e_lpsel
#undef e_rpsel
#undef e_tsel
#undef e_ltsel
#undef e_rtsel
#undef e_one
#undef e_two
#undef e_pcrel
#undef e_con
#undef e_plabel
#undef e_abs

/* for compatibility */
enum hppa_reloc_field_selector_type_alt
  {
    e_fsel = R_HPPA_FSEL,
    e_lssel = R_HPPA_LSSEL,
    e_rssel = R_HPPA_RSSEL,
    e_lsel = R_HPPA_LSEL,
    e_rsel = R_HPPA_RSEL,
    e_ldsel = R_HPPA_LDSEL,
    e_rdsel = R_HPPA_RDSEL,
    e_lrsel = R_HPPA_LRSEL,
    e_rrsel = R_HPPA_RRSEL,
    e_nsel = R_HPPA_NSEL,
    e_nlsel = R_HPPA_NLSEL,
    e_nlrsel = R_HPPA_NLRSEL,
    e_psel = R_HPPA_PSEL,
    e_lpsel = R_HPPA_LPSEL,
    e_rpsel = R_HPPA_RPSEL,
    e_tsel = R_HPPA_TSEL,
    e_ltsel = R_HPPA_LTSEL,
    e_rtsel = R_HPPA_RTSEL
  };

enum hppa_reloc_expr_type
  {
    R_HPPA_E_ONE = 0,
    R_HPPA_E_TWO = 1,
    R_HPPA_E_PCREL = 2,
    R_HPPA_E_CON = 3,
    R_HPPA_E_PLABEL = 7,
    R_HPPA_E_ABS = 18
  };

/* for compatibility */
enum hppa_reloc_expr_type_alt
  {
    e_one = R_HPPA_E_ONE,
    e_two = R_HPPA_E_TWO,
    e_pcrel = R_HPPA_E_PCREL,
    e_con = R_HPPA_E_CON,
    e_plabel = R_HPPA_E_PLABEL,
    e_abs = R_HPPA_E_ABS
  };


/* Relocations for function calls must be accompanied by parameter
   relocation bits.  These bits describe exactly where the caller has
   placed the function's arguments and where it expects to find a return
   value.

   Both ELF and SOM encode this information within the addend field
   of the call relocation.  (Note this could break very badly if one
   was to make a call like bl foo + 0x12345678).

   The high order 10 bits contain parameter relocation information,
   the low order 22 bits contain the constant offset.  */
   
#define HPPA_R_ARG_RELOC(a)	(((a) >> 22) & 0x3FF)
#define HPPA_R_CONSTANT(a)	((((int)(a)) << 10) >> 10)
#define HPPA_R_ADDEND(r,c)	(((r) << 22) + ((c) & 0x3FFFFF))

/* Some functions to manipulate PA instructions.  */
static INLINE unsigned int
assemble_3 (x)
     unsigned int x;
{
  return (((x & 1) << 2) | ((x & 6) >> 1)) & 7;
}

static INLINE void
dis_assemble_3 (x, r)
     unsigned int x;
     unsigned int *r;
{
  *r = (((x & 4) >> 2) | ((x & 3) << 1)) & 7;
}

static INLINE unsigned int
assemble_12 (x, y)
     unsigned int x, y;
{
  return (((y & 1) << 11) | ((x & 1) << 10) | ((x & 0x7fe) >> 1)) & 0xfff;
}

static INLINE void
dis_assemble_12 (as12, x, y)
     unsigned int as12;
     unsigned int *x, *y;
{
  *y = (as12 & 0x800) >> 11;
  *x = ((as12 & 0x3ff) << 1) | ((as12 & 0x400) >> 10);
}

static INLINE unsigned long
assemble_17 (x, y, z)
     unsigned int x, y, z;
{
  unsigned long temp;

  temp = ((z & 1) << 16) |
    ((x & 0x1f) << 11) |
    ((y & 1) << 10) |
    ((y & 0x7fe) >> 1);
  return temp & 0x1ffff;
}

static INLINE void
dis_assemble_17 (as17, x, y, z)
     unsigned int as17;
     unsigned int *x, *y, *z;
{

  *z = (as17 & 0x10000) >> 16;
  *x = (as17 & 0x0f800) >> 11;
  *y = (((as17 & 0x00400) >> 10) | ((as17 & 0x3ff) << 1)) & 0x7ff;
}

static INLINE unsigned long
assemble_21 (x)
     unsigned int x;
{
  unsigned long temp;

  temp = ((x & 1) << 20) |
    ((x & 0xffe) << 8) |
    ((x & 0xc000) >> 7) |
    ((x & 0x1f0000) >> 14) |
    ((x & 0x003000) >> 12);
  return temp & 0x1fffff;
}

static INLINE void
dis_assemble_21 (as21, x)
     unsigned int as21, *x;
{
  unsigned long temp;


  temp = (as21 & 0x100000) >> 20;
  temp |= (as21 & 0x0ffe00) >> 8;
  temp |= (as21 & 0x000180) << 7;
  temp |= (as21 & 0x00007c) << 14;
  temp |= (as21 & 0x000003) << 12;
  *x = temp;
}

static INLINE unsigned long
sign_extend (x, len)
     unsigned int x, len;
{
  return (int)(x >> (len - 1) ? (-1 << len) | x : x);
}

static INLINE unsigned int
ones (n)
     int n;
{
  unsigned int len_ones;
  int i;

  i = 0;
  len_ones = 0;
  while (i < n)
    {
      len_ones = (len_ones << 1) | 1;
      i++;
    }

  return len_ones;
}

static INLINE void
sign_unext (x, len, result)
     unsigned int x, len;
     unsigned int *result;
{
  unsigned int len_ones;

  len_ones = ones (len);

  *result = x & len_ones;
}

static INLINE unsigned long
low_sign_extend (x, len)
     unsigned int x, len;
{
  return (int)((x & 0x1 ? (-1 << (len - 1)) : 0) | x >> 1);
}

static INLINE void
low_sign_unext (x, len, result)
     unsigned int x, len;
     unsigned int *result;
{
  unsigned int temp;
  unsigned int sign;
  unsigned int rest;
  unsigned int one_bit_at_len;
  unsigned int len_ones;

  len_ones = ones (len);
  one_bit_at_len = 1 << (len - 1);

  sign_unext (x, len, &temp);
  sign = temp & one_bit_at_len;
  sign >>= (len - 1);

  rest = temp & (len_ones ^ one_bit_at_len);
  rest <<= 1;

  *result = rest | sign;
}

/* Handle field selectors for PA instructions.  */

static INLINE unsigned long
hppa_field_adjust (value, constant_value, r_field)
     unsigned long value;
     unsigned long constant_value;
     unsigned short r_field;
{
  switch (r_field)
    {
    case e_fsel:		/* F  : no change                      */
    case e_nsel:		/* N  : no change		       */
      value += constant_value;
      break;

    case e_lssel:		/* LS : if (bit 21) then add 0x800
				   arithmetic shift right 11 bits */
      value += constant_value;
      if (value & 0x00000400)
	value += 0x800;
      value = (value & 0xfffff800) >> 11;
      break;

    case e_rssel:		/* RS : Sign extend from bit 21        */
      value += constant_value;
      if (value & 0x00000400)
	value |= 0xfffff800;
      else
	value &= 0x7ff;
      break;

    case e_lsel:		/* L  : Arithmetic shift right 11 bits */
    case e_nlsel:		/* NL  : Arithmetic shift right 11 bits */
      value += constant_value;
      value = (value & 0xfffff800) >> 11;
      break;

    case e_rsel:		/* R  : Set bits 0-20 to zero          */
      value += constant_value;
      value = value & 0x7ff;
      break;

    case e_ldsel:		/* LD : Add 0x800, arithmetic shift
				   right 11 bits                  */
      value += constant_value;
      value += 0x800;
      value = (value & 0xfffff800) >> 11;
      break;

    case e_rdsel:		/* RD : Set bits 0-20 to one           */
      value += constant_value;
      value |= 0xfffff800;
      break;

    case e_lrsel:		/* LR : L with "rounded" constant      */
    case e_nlrsel:		/* NLR : NL with "rounded" constant      */
      value = value + ((constant_value + 0x1000) & 0xffffe000);
      value = (value & 0xfffff800) >> 11;
      break;

    case e_rrsel:		/* RR : R with "rounded" constant      */
      value = value + ((constant_value + 0x1000) & 0xffffe000);
      value = (value & 0x7ff) + constant_value - ((constant_value + 0x1000) & 0xffffe000);
      break;

    default:
      abort ();
    }
  return value;

}

/* PA-RISC OPCODES */
#define get_opcode(insn)	((insn) & 0xfc000000) >> 26

/* FIXME: this list is incomplete.  It should also be an enumerated
   type rather than #defines.  */

#define LDO	0x0d
#define LDB	0x10
#define LDH	0x11
#define LDW	0x12
#define LDWM	0x13
#define STB	0x18
#define STH	0x19
#define STW	0x1a
#define STWM	0x1b
#define COMICLR	0x24
#define SUBI	0x25
#define SUBIO	0x25
#define ADDIT	0x2c
#define ADDITO	0x2c
#define ADDI	0x2d
#define ADDIO	0x2d
#define LDIL	0x08
#define ADDIL	0x0a

#define MOVB	0x32
#define MOVIB	0x33
#define COMBT	0x20
#define COMBF	0x22
#define COMIBT	0x21
#define COMIBF	0x23
#define ADDBT	0x28
#define ADDBF	0x2a
#define ADDIBT	0x29
#define ADDIBF	0x2b
#define BVB	0x30
#define BB	0x31

#define BL	0x3a
#define BLE	0x39
#define BE	0x38

  
/* Given a machine instruction, return its format.

   FIXME:  opcodes which do not map to a known format
   should return an error of some sort.  */

static INLINE char
bfd_hppa_insn2fmt (insn)
     unsigned long insn;
{
  char fmt = -1;
  unsigned char op = get_opcode (insn);
  
  switch (op)
    {
    case ADDI:
    case ADDIT:
    case SUBI:
      fmt = 11;
      break;
    case MOVB:
    case MOVIB:
    case COMBT:
    case COMBF:
    case COMIBT:
    case COMIBF:
    case ADDBT:
    case ADDBF:
    case ADDIBT:
    case ADDIBF:
    case BVB:
    case BB:
      fmt = 12;
      break;
    case LDO:
    case LDB:
    case LDH:
    case LDW:
    case LDWM:
    case STB:
    case STH:
    case STW:
    case STWM:
      fmt = 14;
      break;
    case BL:
    case BE:
    case BLE:
      fmt = 17;
      break;
    case LDIL:
    case ADDIL:
      fmt = 21;
      break;
    default:
      fmt = 32;
      break;
    }
  return fmt;
}


/* Insert VALUE into INSN using R_FORMAT to determine exactly what
   bits to change.  */
   
static INLINE unsigned long
hppa_rebuild_insn (abfd, insn, value, r_format)
     bfd *abfd;
     unsigned long insn;
     unsigned long value;
     unsigned long r_format;
{
  unsigned long const_part;
  unsigned long rebuilt_part;

  switch (r_format)
    {
    case 11:
      {
	unsigned w1, w;

	const_part = insn & 0xffffe002;
	dis_assemble_12 (value, &w1, &w);
	rebuilt_part = (w1 << 2) | w;
	return const_part | rebuilt_part;
      }

    case 12:
      {
	unsigned w1, w;

	const_part = insn & 0xffffe002;
	dis_assemble_12 (value, &w1, &w);
	rebuilt_part = (w1 << 2) | w;
	return const_part | rebuilt_part;
      }

    case 14:
      const_part = insn & 0xffffc000;
      low_sign_unext (value, 14, &rebuilt_part);
      return const_part | rebuilt_part;

    case 17:
      {
	unsigned w1, w2, w;

	const_part = insn & 0xffe0e002;
	dis_assemble_17 (value, &w1, &w2, &w);
	rebuilt_part = (w2 << 2) | (w1 << 16) | w;
	return const_part | rebuilt_part;
      }

    case 21:
      const_part = insn & 0xffe00000;
      dis_assemble_21 (value, &rebuilt_part);
      return const_part | rebuilt_part;

    case 32:
      const_part = 0;
      return value;

    default:
      abort ();
    }
  return insn;
}

#endif /* _HPPA_H */
