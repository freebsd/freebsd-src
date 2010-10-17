/* tc-sh.c -- Assemble code for the Renesas / SuperH SH
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004  Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Written By Steve Chamberlain <sac@cygnus.com>  */

#include <stdio.h>
#include "as.h"
#include "bfd.h"
#include "subsegs.h"
#define DEFINE_TABLE
#include "opcodes/sh-opc.h"
#include "safe-ctype.h"
#include "struc-symbol.h"

#ifdef OBJ_ELF
#include "elf/sh.h"
#endif

#include "dwarf2dbg.h"
#include "dw2gencfi.h"

typedef struct
  {
    sh_arg_type type;
    int reg;
    expressionS immediate;
  }
sh_operand_info;

const char comment_chars[] = "!";
const char line_separator_chars[] = ";";
const char line_comment_chars[] = "!#";

static void s_uses (int);
static void s_uacons (int);

#ifdef OBJ_ELF
static void sh_elf_cons (int);

symbolS *GOT_symbol;		/* Pre-defined "_GLOBAL_OFFSET_TABLE_" */
#endif

static void
big (int ignore ATTRIBUTE_UNUSED)
{
  if (! target_big_endian)
    as_bad (_("directive .big encountered when option -big required"));

  /* Stop further messages.  */
  target_big_endian = 1;
}

static void
little (int ignore ATTRIBUTE_UNUSED)
{
  if (target_big_endian)
    as_bad (_("directive .little encountered when option -little required"));

  /* Stop further messages.  */
  target_big_endian = 0;
}

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function.  */

const pseudo_typeS md_pseudo_table[] =
{
#ifdef OBJ_ELF
  {"long", sh_elf_cons, 4},
  {"int", sh_elf_cons, 4},
  {"word", sh_elf_cons, 2},
  {"short", sh_elf_cons, 2},
#else
  {"int", cons, 4},
  {"word", cons, 2},
#endif /* OBJ_ELF */
  {"big", big, 0},
  {"form", listing_psize, 0},
  {"little", little, 0},
  {"heading", listing_title, 0},
  {"import", s_ignore, 0},
  {"page", listing_eject, 0},
  {"program", s_ignore, 0},
  {"uses", s_uses, 0},
  {"uaword", s_uacons, 2},
  {"ualong", s_uacons, 4},
  {"uaquad", s_uacons, 8},
  {"2byte", s_uacons, 2},
  {"4byte", s_uacons, 4},
  {"8byte", s_uacons, 8},
#ifdef HAVE_SH64
  {"mode", s_sh64_mode, 0 },

  /* Have the old name too.  */
  {"isa", s_sh64_mode, 0 },

  /* Assert that the right ABI is used.  */
  {"abi", s_sh64_abi, 0 },

  { "vtable_inherit", sh64_vtable_inherit, 0 },
  { "vtable_entry", sh64_vtable_entry, 0 },
#endif /* HAVE_SH64 */
  {0, 0, 0}
};

/*int md_reloc_size; */

int sh_relax;		/* set if -relax seen */

/* Whether -small was seen.  */

int sh_small;

/* Flag to generate relocations against symbol values for local symbols.  */

static int dont_adjust_reloc_32;

/* preset architecture set, if given; zero otherwise.  */

static int preset_target_arch;

/* The bit mask of architectures that could
   accommodate the insns seen so far.  */
static int valid_arch;

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

#define C(a,b) ENCODE_RELAX(a,b)

#define ENCODE_RELAX(what,length) (((what) << 4) + (length))
#define GET_WHAT(x) ((x>>4))

/* These are the three types of relaxable instruction.  */
/* These are the types of relaxable instructions; except for END which is
   a marker.  */
#define COND_JUMP 1
#define COND_JUMP_DELAY 2
#define UNCOND_JUMP  3

#ifdef HAVE_SH64

/* A 16-bit (times four) pc-relative operand, at most expanded to 32 bits.  */
#define SH64PCREL16_32 4
/* A 16-bit (times four) pc-relative operand, at most expanded to 64 bits.  */
#define SH64PCREL16_64 5

/* Variants of the above for adjusting the insn to PTA or PTB according to
   the label.  */
#define SH64PCREL16PT_32 6
#define SH64PCREL16PT_64 7

/* A MOVI expansion, expanding to at most 32 or 64 bits.  */
#define MOVI_IMM_32 8
#define MOVI_IMM_32_PCREL 9
#define MOVI_IMM_64 10
#define MOVI_IMM_64_PCREL 11
#define END 12

#else  /* HAVE_SH64 */

#define END 4

#endif /* HAVE_SH64 */

#define UNDEF_DISP 0
#define COND8  1
#define COND12 2
#define COND32 3
#define UNDEF_WORD_DISP 4

#define UNCOND12 1
#define UNCOND32 2

#ifdef HAVE_SH64
#define UNDEF_SH64PCREL 0
#define SH64PCREL16 1
#define SH64PCREL32 2
#define SH64PCREL48 3
#define SH64PCREL64 4
#define SH64PCRELPLT 5

#define UNDEF_MOVI 0
#define MOVI_16 1
#define MOVI_32 2
#define MOVI_48 3
#define MOVI_64 4
#define MOVI_PLT 5
#define MOVI_GOTOFF 6
#define MOVI_GOTPC 7
#endif /* HAVE_SH64 */

/* Branch displacements are from the address of the branch plus
   four, thus all minimum and maximum values have 4 added to them.  */
#define COND8_F 258
#define COND8_M -252
#define COND8_LENGTH 2

/* There is one extra instruction before the branch, so we must add
   two more bytes to account for it.  */
#define COND12_F 4100
#define COND12_M -4090
#define COND12_LENGTH 6

#define COND12_DELAY_LENGTH 4

/* ??? The minimum and maximum values are wrong, but this does not matter
   since this relocation type is not supported yet.  */
#define COND32_F (1<<30)
#define COND32_M -(1<<30)
#define COND32_LENGTH 14

#define UNCOND12_F 4098
#define UNCOND12_M -4092
#define UNCOND12_LENGTH 2

/* ??? The minimum and maximum values are wrong, but this does not matter
   since this relocation type is not supported yet.  */
#define UNCOND32_F (1<<30)
#define UNCOND32_M -(1<<30)
#define UNCOND32_LENGTH 14

#ifdef HAVE_SH64
/* The trivial expansion of a SH64PCREL16 relaxation is just a "PT label,
   TRd" as is the current insn, so no extra length.  Note that the "reach"
   is calculated from the address *after* that insn, but the offset in the
   insn is calculated from the beginning of the insn.  We also need to
   take into account the implicit 1 coded as the "A" in PTA when counting
   forward.  If PTB reaches an odd address, we trap that as an error
   elsewhere, so we don't have to have different relaxation entries.  We
   don't add a one to the negative range, since PTB would then have the
   farthest backward-reaching value skipped, not generated at relaxation.  */
#define SH64PCREL16_F (32767 * 4 - 4 + 1)
#define SH64PCREL16_M (-32768 * 4 - 4)
#define SH64PCREL16_LENGTH 0

/* The next step is to change that PT insn into
     MOVI ((label - datalabel Ln) >> 16) & 65535, R25
     SHORI (label - datalabel Ln) & 65535, R25
    Ln:
     PTREL R25,TRd
   which means two extra insns, 8 extra bytes.  This is the limit for the
   32-bit ABI.

   The expressions look a bit bad since we have to adjust this to avoid overflow on a
   32-bit host.  */
#define SH64PCREL32_F ((((long) 1 << 30) - 1) * 2 + 1 - 4)
#define SH64PCREL32_LENGTH (2 * 4)

/* Similarly, we just change the MOVI and add a SHORI for the 48-bit
   expansion.  */
#if BFD_HOST_64BIT_LONG
/* The "reach" type is long, so we can only do this for a 64-bit-long
   host.  */
#define SH64PCREL32_M (((long) -1 << 30) * 2 - 4)
#define SH64PCREL48_F ((((long) 1 << 47) - 1) - 4)
#define SH64PCREL48_M (((long) -1 << 47) - 4)
#define SH64PCREL48_LENGTH (3 * 4)
#else
/* If the host does not have 64-bit longs, just make this state identical
   in reach to the 32-bit state.  Note that we have a slightly incorrect
   reach, but the correct one above will overflow a 32-bit number.  */
#define SH64PCREL32_M (((long) -1 << 30) * 2)
#define SH64PCREL48_F SH64PCREL32_F
#define SH64PCREL48_M SH64PCREL32_M
#define SH64PCREL48_LENGTH (3 * 4)
#endif /* BFD_HOST_64BIT_LONG */

/* And similarly for the 64-bit expansion; a MOVI + SHORI + SHORI + SHORI
   + PTREL sequence.  */
#define SH64PCREL64_LENGTH (4 * 4)

/* For MOVI, we make the MOVI + SHORI... expansion you can see in the
   SH64PCREL expansions.  The PCREL one is similar, but the other has no
   pc-relative reach; it must be fully expanded in
   shmedia_md_estimate_size_before_relax.  */
#define MOVI_16_LENGTH 0
#define MOVI_16_F (32767 - 4)
#define MOVI_16_M (-32768 - 4)
#define MOVI_32_LENGTH 4
#define MOVI_32_F ((((long) 1 << 30) - 1) * 2 + 1 - 4)
#define MOVI_48_LENGTH 8

#if BFD_HOST_64BIT_LONG
/* The "reach" type is long, so we can only do this for a 64-bit-long
   host.  */
#define MOVI_32_M (((long) -1 << 30) * 2 - 4)
#define MOVI_48_F ((((long) 1 << 47) - 1) - 4)
#define MOVI_48_M (((long) -1 << 47) - 4)
#else
/* If the host does not have 64-bit longs, just make this state identical
   in reach to the 32-bit state.  Note that we have a slightly incorrect
   reach, but the correct one above will overflow a 32-bit number.  */
#define MOVI_32_M (((long) -1 << 30) * 2)
#define MOVI_48_F MOVI_32_F
#define MOVI_48_M MOVI_32_M
#endif /* BFD_HOST_64BIT_LONG */

#define MOVI_64_LENGTH 12
#endif /* HAVE_SH64 */

#define EMPTY { 0, 0, 0, 0 }

const relax_typeS md_relax_table[C (END, 0)] = {
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  EMPTY,
  /* C (COND_JUMP, COND8) */
  { COND8_F, COND8_M, COND8_LENGTH, C (COND_JUMP, COND12) },
  /* C (COND_JUMP, COND12) */
  { COND12_F, COND12_M, COND12_LENGTH, C (COND_JUMP, COND32), },
  /* C (COND_JUMP, COND32) */
  { COND32_F, COND32_M, COND32_LENGTH, 0, },
  /* C (COND_JUMP, UNDEF_WORD_DISP) */
  { 0, 0, COND32_LENGTH, 0, },
  EMPTY, EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  EMPTY,
  /* C (COND_JUMP_DELAY, COND8) */
  { COND8_F, COND8_M, COND8_LENGTH, C (COND_JUMP_DELAY, COND12) },
  /* C (COND_JUMP_DELAY, COND12) */
  { COND12_F, COND12_M, COND12_DELAY_LENGTH, C (COND_JUMP_DELAY, COND32), },
  /* C (COND_JUMP_DELAY, COND32) */
  { COND32_F, COND32_M, COND32_LENGTH, 0, },
  /* C (COND_JUMP_DELAY, UNDEF_WORD_DISP) */
  { 0, 0, COND32_LENGTH, 0, },
  EMPTY, EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  EMPTY,
  /* C (UNCOND_JUMP, UNCOND12) */
  { UNCOND12_F, UNCOND12_M, UNCOND12_LENGTH, C (UNCOND_JUMP, UNCOND32), },
  /* C (UNCOND_JUMP, UNCOND32) */
  { UNCOND32_F, UNCOND32_M, UNCOND32_LENGTH, 0, },
  EMPTY,
  /* C (UNCOND_JUMP, UNDEF_WORD_DISP) */
  { 0, 0, UNCOND32_LENGTH, 0, },
  EMPTY, EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

#ifdef HAVE_SH64
  /* C (SH64PCREL16_32, SH64PCREL16) */
  EMPTY,
  { SH64PCREL16_F, SH64PCREL16_M, SH64PCREL16_LENGTH, C (SH64PCREL16_32, SH64PCREL32) },
  /* C (SH64PCREL16_32, SH64PCREL32) */
  { 0, 0, SH64PCREL32_LENGTH, 0 },
  EMPTY, EMPTY,
  /* C (SH64PCREL16_32, SH64PCRELPLT) */
  { 0, 0, SH64PCREL32_LENGTH, 0 },
  EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (SH64PCREL16_64, SH64PCREL16) */
  EMPTY,
  { SH64PCREL16_F, SH64PCREL16_M, SH64PCREL16_LENGTH, C (SH64PCREL16_64, SH64PCREL32) },
  /* C (SH64PCREL16_64, SH64PCREL32) */
  { SH64PCREL32_F, SH64PCREL32_M, SH64PCREL32_LENGTH, C (SH64PCREL16_64, SH64PCREL48) },
  /* C (SH64PCREL16_64, SH64PCREL48) */
  { SH64PCREL48_F, SH64PCREL48_M, SH64PCREL48_LENGTH, C (SH64PCREL16_64, SH64PCREL64) },
  /* C (SH64PCREL16_64, SH64PCREL64) */
  { 0, 0, SH64PCREL64_LENGTH, 0 },
  /* C (SH64PCREL16_64, SH64PCRELPLT) */
  { 0, 0, SH64PCREL64_LENGTH, 0 },
  EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (SH64PCREL16PT_32, SH64PCREL16) */
  EMPTY,
  { SH64PCREL16_F, SH64PCREL16_M, SH64PCREL16_LENGTH, C (SH64PCREL16PT_32, SH64PCREL32) },
  /* C (SH64PCREL16PT_32, SH64PCREL32) */
  { 0, 0, SH64PCREL32_LENGTH, 0 },
  EMPTY, EMPTY,
  /* C (SH64PCREL16PT_32, SH64PCRELPLT) */
  { 0, 0, SH64PCREL32_LENGTH, 0 },
  EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (SH64PCREL16PT_64, SH64PCREL16) */
  EMPTY,
  { SH64PCREL16_F, SH64PCREL16_M, SH64PCREL16_LENGTH, C (SH64PCREL16PT_64, SH64PCREL32) },
  /* C (SH64PCREL16PT_64, SH64PCREL32) */
  { SH64PCREL32_F,
    SH64PCREL32_M,
    SH64PCREL32_LENGTH,
    C (SH64PCREL16PT_64, SH64PCREL48) },
  /* C (SH64PCREL16PT_64, SH64PCREL48) */
  { SH64PCREL48_F, SH64PCREL48_M, SH64PCREL48_LENGTH, C (SH64PCREL16PT_64, SH64PCREL64) },
  /* C (SH64PCREL16PT_64, SH64PCREL64) */
  { 0, 0, SH64PCREL64_LENGTH, 0 },
  /* C (SH64PCREL16PT_64, SH64PCRELPLT) */
  { 0, 0, SH64PCREL64_LENGTH, 0},
  EMPTY, EMPTY,
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (MOVI_IMM_32, UNDEF_MOVI) */
  { 0, 0, MOVI_32_LENGTH, 0 },
  /* C (MOVI_IMM_32, MOVI_16) */
  { MOVI_16_F, MOVI_16_M, MOVI_16_LENGTH, C (MOVI_IMM_32, MOVI_32) },
  /* C (MOVI_IMM_32, MOVI_32) */
  { MOVI_32_F, MOVI_32_M, MOVI_32_LENGTH, 0 },
  EMPTY, EMPTY, EMPTY,
  /* C (MOVI_IMM_32, MOVI_GOTOFF) */
  { 0, 0, MOVI_32_LENGTH, 0 },
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (MOVI_IMM_32_PCREL, MOVI_16) */
  EMPTY,
  { MOVI_16_F, MOVI_16_M, MOVI_16_LENGTH, C (MOVI_IMM_32_PCREL, MOVI_32) },
  /* C (MOVI_IMM_32_PCREL, MOVI_32) */
  { 0, 0, MOVI_32_LENGTH, 0 },
  EMPTY, EMPTY,
  /* C (MOVI_IMM_32_PCREL, MOVI_PLT) */
  { 0, 0, MOVI_32_LENGTH, 0 },
  EMPTY,
  /* C (MOVI_IMM_32_PCREL, MOVI_GOTPC) */
  { 0, 0, MOVI_32_LENGTH, 0 },
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (MOVI_IMM_64, UNDEF_MOVI) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  /* C (MOVI_IMM_64, MOVI_16) */
  { MOVI_16_F, MOVI_16_M, MOVI_16_LENGTH, C (MOVI_IMM_64, MOVI_32) },
  /* C (MOVI_IMM_64, MOVI_32) */
  { MOVI_32_F, MOVI_32_M, MOVI_32_LENGTH, C (MOVI_IMM_64, MOVI_48) },
  /* C (MOVI_IMM_64, MOVI_48) */
  { MOVI_48_F, MOVI_48_M, MOVI_48_LENGTH, C (MOVI_IMM_64, MOVI_64) },
  /* C (MOVI_IMM_64, MOVI_64) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  EMPTY,
  /* C (MOVI_IMM_64, MOVI_GOTOFF) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

  /* C (MOVI_IMM_64_PCREL, MOVI_16) */
  EMPTY,
  { MOVI_16_F, MOVI_16_M, MOVI_16_LENGTH, C (MOVI_IMM_64_PCREL, MOVI_32) },
  /* C (MOVI_IMM_64_PCREL, MOVI_32) */
  { MOVI_32_F, MOVI_32_M, MOVI_32_LENGTH, C (MOVI_IMM_64_PCREL, MOVI_48) },
  /* C (MOVI_IMM_64_PCREL, MOVI_48) */
  { MOVI_48_F, MOVI_48_M, MOVI_48_LENGTH, C (MOVI_IMM_64_PCREL, MOVI_64) },
  /* C (MOVI_IMM_64_PCREL, MOVI_64) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  /* C (MOVI_IMM_64_PCREL, MOVI_PLT) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  EMPTY,
  /* C (MOVI_IMM_64_PCREL, MOVI_GOTPC) */
  { 0, 0, MOVI_64_LENGTH, 0 },
  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

#endif /* HAVE_SH64 */

};

#undef EMPTY

static struct hash_control *opcode_hash_control;	/* Opcode mnemonics */


#ifdef OBJ_ELF
/* Determinet whether the symbol needs any kind of PIC relocation.  */

inline static int
sh_PIC_related_p (symbolS *sym)
{
  expressionS *exp;

  if (! sym)
    return 0;

  if (sym == GOT_symbol)
    return 1;

#ifdef HAVE_SH64
  if (sh_PIC_related_p (*symbol_get_tc (sym)))
    return 1;
#endif

  exp = symbol_get_value_expression (sym);

  return (exp->X_op == O_PIC_reloc
	  || sh_PIC_related_p (exp->X_add_symbol)
	  || sh_PIC_related_p (exp->X_op_symbol));
}

/* Determine the relocation type to be used to represent the
   expression, that may be rearranged.  */

static int
sh_check_fixup (expressionS *main_exp, bfd_reloc_code_real_type *r_type_p)
{
  expressionS *exp = main_exp;

  /* This is here for backward-compatibility only.  GCC used to generated:

	f@PLT + . - (.LPCS# + 2)

     but we'd rather be able to handle this as a PIC-related reference
     plus/minus a symbol.  However, gas' parser gives us:

	O_subtract (O_add (f@PLT, .), .LPCS#+2)

     so we attempt to transform this into:

        O_subtract (f@PLT, O_subtract (.LPCS#+2, .))

     which we can handle simply below.  */
  if (exp->X_op == O_subtract)
    {
      if (sh_PIC_related_p (exp->X_op_symbol))
	return 1;

      exp = symbol_get_value_expression (exp->X_add_symbol);

      if (exp && sh_PIC_related_p (exp->X_op_symbol))
	return 1;

      if (exp && exp->X_op == O_add
	  && sh_PIC_related_p (exp->X_add_symbol))
	{
	  symbolS *sym = exp->X_add_symbol;

	  exp->X_op = O_subtract;
	  exp->X_add_symbol = main_exp->X_op_symbol;

	  main_exp->X_op_symbol = main_exp->X_add_symbol;
	  main_exp->X_add_symbol = sym;

	  main_exp->X_add_number += exp->X_add_number;
	  exp->X_add_number = 0;
	}

      exp = main_exp;
    }
  else if (exp->X_op == O_add && sh_PIC_related_p (exp->X_op_symbol))
    return 1;

  if (exp->X_op == O_symbol || exp->X_op == O_add || exp->X_op == O_subtract)
    {
#ifdef HAVE_SH64
      if (exp->X_add_symbol
	  && (exp->X_add_symbol == GOT_symbol
	      || (GOT_symbol
		  && *symbol_get_tc (exp->X_add_symbol) == GOT_symbol)))
	{
	  switch (*r_type_p)
	    {
	    case BFD_RELOC_SH_IMM_LOW16:
	      *r_type_p = BFD_RELOC_SH_GOTPC_LOW16;
	      break;

	    case BFD_RELOC_SH_IMM_MEDLOW16:
	      *r_type_p = BFD_RELOC_SH_GOTPC_MEDLOW16;
	      break;

	    case BFD_RELOC_SH_IMM_MEDHI16:
	      *r_type_p = BFD_RELOC_SH_GOTPC_MEDHI16;
	      break;

	    case BFD_RELOC_SH_IMM_HI16:
	      *r_type_p = BFD_RELOC_SH_GOTPC_HI16;
	      break;

	    case BFD_RELOC_NONE:
	    case BFD_RELOC_UNUSED:
	      *r_type_p = BFD_RELOC_SH_GOTPC;
	      break;

	    default:
	      abort ();
	    }
	  return 0;
	}
#else
      if (exp->X_add_symbol && exp->X_add_symbol == GOT_symbol)
	{
	  *r_type_p = BFD_RELOC_SH_GOTPC;
	  return 0;
	}
#endif
      exp = symbol_get_value_expression (exp->X_add_symbol);
      if (! exp)
	return 0;
    }

  if (exp->X_op == O_PIC_reloc)
    {
#ifdef HAVE_SH64
      switch (*r_type_p)
	{
	case BFD_RELOC_NONE:
	case BFD_RELOC_UNUSED:
	  *r_type_p = exp->X_md;
	  break;

	case BFD_RELOC_SH_IMM_LOW16:
	  switch (exp->X_md)
	    {
	    case BFD_RELOC_32_GOTOFF:
	      *r_type_p = BFD_RELOC_SH_GOTOFF_LOW16;
	      break;

	    case BFD_RELOC_SH_GOTPLT32:
	      *r_type_p = BFD_RELOC_SH_GOTPLT_LOW16;
	      break;

	    case BFD_RELOC_32_GOT_PCREL:
	      *r_type_p = BFD_RELOC_SH_GOT_LOW16;
	      break;

	    case BFD_RELOC_32_PLT_PCREL:
	      *r_type_p = BFD_RELOC_SH_PLT_LOW16;
	      break;

	    default:
	      abort ();
	    }
	  break;

	case BFD_RELOC_SH_IMM_MEDLOW16:
	  switch (exp->X_md)
	    {
	    case BFD_RELOC_32_GOTOFF:
	      *r_type_p = BFD_RELOC_SH_GOTOFF_MEDLOW16;
	      break;

	    case BFD_RELOC_SH_GOTPLT32:
	      *r_type_p = BFD_RELOC_SH_GOTPLT_MEDLOW16;
	      break;

	    case BFD_RELOC_32_GOT_PCREL:
	      *r_type_p = BFD_RELOC_SH_GOT_MEDLOW16;
	      break;

	    case BFD_RELOC_32_PLT_PCREL:
	      *r_type_p = BFD_RELOC_SH_PLT_MEDLOW16;
	      break;

	    default:
	      abort ();
	    }
	  break;

	case BFD_RELOC_SH_IMM_MEDHI16:
	  switch (exp->X_md)
	    {
	    case BFD_RELOC_32_GOTOFF:
	      *r_type_p = BFD_RELOC_SH_GOTOFF_MEDHI16;
	      break;

	    case BFD_RELOC_SH_GOTPLT32:
	      *r_type_p = BFD_RELOC_SH_GOTPLT_MEDHI16;
	      break;

	    case BFD_RELOC_32_GOT_PCREL:
	      *r_type_p = BFD_RELOC_SH_GOT_MEDHI16;
	      break;

	    case BFD_RELOC_32_PLT_PCREL:
	      *r_type_p = BFD_RELOC_SH_PLT_MEDHI16;
	      break;

	    default:
	      abort ();
	    }
	  break;

	case BFD_RELOC_SH_IMM_HI16:
	  switch (exp->X_md)
	    {
	    case BFD_RELOC_32_GOTOFF:
	      *r_type_p = BFD_RELOC_SH_GOTOFF_HI16;
	      break;

	    case BFD_RELOC_SH_GOTPLT32:
	      *r_type_p = BFD_RELOC_SH_GOTPLT_HI16;
	      break;

	    case BFD_RELOC_32_GOT_PCREL:
	      *r_type_p = BFD_RELOC_SH_GOT_HI16;
	      break;

	    case BFD_RELOC_32_PLT_PCREL:
	      *r_type_p = BFD_RELOC_SH_PLT_HI16;
	      break;

	    default:
	      abort ();
	    }
	  break;

	default:
	  abort ();
	}
#else
      *r_type_p = exp->X_md;
#endif
      if (exp == main_exp)
	exp->X_op = O_symbol;
      else
	{
	  main_exp->X_add_symbol = exp->X_add_symbol;
	  main_exp->X_add_number += exp->X_add_number;
	}
    }
  else
    return (sh_PIC_related_p (exp->X_add_symbol)
	    || sh_PIC_related_p (exp->X_op_symbol));

  return 0;
}

/* Add expression EXP of SIZE bytes to offset OFF of fragment FRAG.  */

void
sh_cons_fix_new (fragS *frag, int off, int size, expressionS *exp)
{
  bfd_reloc_code_real_type r_type = BFD_RELOC_UNUSED;

  if (sh_check_fixup (exp, &r_type))
    as_bad (_("Invalid PIC expression."));

  if (r_type == BFD_RELOC_UNUSED)
    switch (size)
      {
      case 1:
	r_type = BFD_RELOC_8;
	break;

      case 2:
	r_type = BFD_RELOC_16;
	break;

      case 4:
	r_type = BFD_RELOC_32;
	break;

#ifdef HAVE_SH64
      case 8:
	r_type = BFD_RELOC_64;
	break;
#endif

      default:
	goto error;
      }
  else if (size != 4)
    {
    error:
      as_bad (_("unsupported BFD relocation size %u"), size);
      r_type = BFD_RELOC_UNUSED;
    }

  fix_new_exp (frag, off, size, exp, 0, r_type);
}

/* The regular cons() function, that reads constants, doesn't support
   suffixes such as @GOT, @GOTOFF and @PLT, that generate
   machine-specific relocation types.  So we must define it here.  */
/* Clobbers input_line_pointer, checks end-of-line.  */
/* NBYTES 1=.byte, 2=.word, 4=.long */
static void
sh_elf_cons (register int nbytes)
{
  expressionS exp;

#ifdef HAVE_SH64

  /* Update existing range to include a previous insn, if there was one.  */
  sh64_update_contents_mark (TRUE);

  /* We need to make sure the contents type is set to data.  */
  sh64_flag_output ();

#endif /* HAVE_SH64 */

  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }

#ifdef md_cons_align
  md_cons_align (nbytes);
#endif

  do
    {
      expression (&exp);
      emit_expr (&exp, (unsigned int) nbytes);
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  if (*input_line_pointer == '#' || *input_line_pointer == '!')
    {
       while (! is_end_of_line[(unsigned char) *input_line_pointer++]);
    }
  else
    demand_empty_rest_of_line ();
}
#endif /* OBJ_ELF */


/* This function is called once, at assembler startup time.  This should
   set up all the tables, etc that the MD part of the assembler needs.  */

void
md_begin (void)
{
  const sh_opcode_info *opcode;
  char *prev_name = "";
  int target_arch;

  target_arch
    = preset_target_arch ? preset_target_arch : arch_sh1_up & ~arch_sh_dsp_up;
  valid_arch = target_arch;

#ifdef HAVE_SH64
  shmedia_md_begin ();
#endif

  opcode_hash_control = hash_new ();

  /* Insert unique names into hash table.  */
  for (opcode = sh_table; opcode->name; opcode++)
    {
      if (strcmp (prev_name, opcode->name) != 0)
	{
	  if (! (opcode->arch & target_arch))
	    continue;
	  prev_name = opcode->name;
	  hash_insert (opcode_hash_control, opcode->name, (char *) opcode);
	}
    }
}

static int reg_m;
static int reg_n;
static int reg_x, reg_y;
static int reg_efg;
static int reg_b;

#define IDENT_CHAR(c) (ISALNUM (c) || (c) == '_')

/* Try to parse a reg name.  Return the number of chars consumed.  */

static int
parse_reg (char *src, int *mode, int *reg)
{
  char l0 = TOLOWER (src[0]);
  char l1 = l0 ? TOLOWER (src[1]) : 0;

  /* We use ! IDENT_CHAR for the next character after the register name, to
     make sure that we won't accidentally recognize a symbol name such as
     'sram' or sr_ram as being a reference to the register 'sr'.  */

  if (l0 == 'r')
    {
      if (l1 == '1')
	{
	  if (src[2] >= '0' && src[2] <= '5'
	      && ! IDENT_CHAR ((unsigned char) src[3]))
	    {
	      *mode = A_REG_N;
	      *reg = 10 + src[2] - '0';
	      return 3;
	    }
	}
      if (l1 >= '0' && l1 <= '9'
	  && ! IDENT_CHAR ((unsigned char) src[2]))
	{
	  *mode = A_REG_N;
	  *reg = (l1 - '0');
	  return 2;
	}
      if (l1 >= '0' && l1 <= '7' && strncasecmp (&src[2], "_bank", 5) == 0
	  && ! IDENT_CHAR ((unsigned char) src[7]))
	{
	  *mode = A_REG_B;
	  *reg  = (l1 - '0');
	  return 7;
	}

      if (l1 == 'e' && ! IDENT_CHAR ((unsigned char) src[2]))
	{
	  *mode = A_RE;
	  return 2;
	}
      if (l1 == 's' && ! IDENT_CHAR ((unsigned char) src[2]))
	{
	  *mode = A_RS;
	  return 2;
	}
    }

  if (l0 == 'a')
    {
      if (l1 == '0')
	{
	  if (! IDENT_CHAR ((unsigned char) src[2]))
	    {
	      *mode = DSP_REG_N;
	      *reg = A_A0_NUM;
	      return 2;
	    }
	  if (TOLOWER (src[2]) == 'g' && ! IDENT_CHAR ((unsigned char) src[3]))
	    {
	      *mode = DSP_REG_N;
	      *reg = A_A0G_NUM;
	      return 3;
	    }
	}
      if (l1 == '1')
	{
	  if (! IDENT_CHAR ((unsigned char) src[2]))
	    {
	      *mode = DSP_REG_N;
	      *reg = A_A1_NUM;
	      return 2;
	    }
	  if (TOLOWER (src[2]) == 'g' && ! IDENT_CHAR ((unsigned char) src[3]))
	    {
	      *mode = DSP_REG_N;
	      *reg = A_A1G_NUM;
	      return 3;
	    }
	}

      if (l1 == 'x' && src[2] >= '0' && src[2] <= '1'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_REG_N;
	  *reg = 4 + (l1 - '0');
	  return 3;
	}
      if (l1 == 'y' && src[2] >= '0' && src[2] <= '1'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_REG_N;
	  *reg = 6 + (l1 - '0');
	  return 3;
	}
      if (l1 == 's' && src[2] >= '0' && src[2] <= '3'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  int n = l1 - '0';

	  *mode = A_REG_N;
	  *reg = n | ((~n & 2) << 1);
	  return 3;
	}
    }

  if (l0 == 'i' && l1 && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      if (l1 == 's')
	{
	  *mode = A_REG_N;
	  *reg = 8;
	  return 2;
	}
      if (l1 == 'x')
	{
	  *mode = A_REG_N;
	  *reg = 8;
	  return 2;
	}
      if (l1 == 'y')
	{
	  *mode = A_REG_N;
	  *reg = 9;
	  return 2;
	}
    }

  if (l0 == 'x' && l1 >= '0' && l1 <= '1'
      && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = DSP_REG_N;
      *reg = A_X0_NUM + l1 - '0';
      return 2;
    }

  if (l0 == 'y' && l1 >= '0' && l1 <= '1'
      && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = DSP_REG_N;
      *reg = A_Y0_NUM + l1 - '0';
      return 2;
    }

  if (l0 == 'm' && l1 >= '0' && l1 <= '1'
      && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = DSP_REG_N;
      *reg = l1 == '0' ? A_M0_NUM : A_M1_NUM;
      return 2;
    }

  if (l0 == 's'
      && l1 == 's'
      && TOLOWER (src[2]) == 'r' && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_SSR;
      return 3;
    }

  if (l0 == 's' && l1 == 'p' && TOLOWER (src[2]) == 'c'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_SPC;
      return 3;
    }

  if (l0 == 's' && l1 == 'g' && TOLOWER (src[2]) == 'r'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_SGR;
      return 3;
    }

  if (l0 == 'd' && l1 == 's' && TOLOWER (src[2]) == 'r'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_DSR;
      return 3;
    }

  if (l0 == 'd' && l1 == 'b' && TOLOWER (src[2]) == 'r'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_DBR;
      return 3;
    }

  if (l0 == 's' && l1 == 'r' && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = A_SR;
      return 2;
    }

  if (l0 == 's' && l1 == 'p' && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = A_REG_N;
      *reg = 15;
      return 2;
    }

  if (l0 == 'p' && l1 == 'r' && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      *mode = A_PR;
      return 2;
    }
  if (l0 == 'p' && l1 == 'c' && ! IDENT_CHAR ((unsigned char) src[2]))
    {
      /* Don't use A_DISP_PC here - that would accept stuff like 'mova pc,r0'
         and use an uninitialized immediate.  */
      *mode = A_PC;
      return 2;
    }
  if (l0 == 'g' && l1 == 'b' && TOLOWER (src[2]) == 'r'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_GBR;
      return 3;
    }
  if (l0 == 'v' && l1 == 'b' && TOLOWER (src[2]) == 'r'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_VBR;
      return 3;
    }

  if (l0 == 'm' && l1 == 'a' && TOLOWER (src[2]) == 'c'
      && ! IDENT_CHAR ((unsigned char) src[4]))
    {
      if (TOLOWER (src[3]) == 'l')
	{
	  *mode = A_MACL;
	  return 4;
	}
      if (TOLOWER (src[3]) == 'h')
	{
	  *mode = A_MACH;
	  return 4;
	}
    }
  if (l0 == 'm' && l1 == 'o' && TOLOWER (src[2]) == 'd'
      && ! IDENT_CHAR ((unsigned char) src[3]))
    {
      *mode = A_MOD;
      return 3;
    }
  if (l0 == 'f' && l1 == 'r')
    {
      if (src[2] == '1')
	{
	  if (src[3] >= '0' && src[3] <= '5'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = F_REG_N;
	      *reg = 10 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '9'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = F_REG_N;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }
  if (l0 == 'd' && l1 == 'r')
    {
      if (src[2] == '1')
	{
	  if (src[3] >= '0' && src[3] <= '4' && ! ((src[3] - '0') & 1)
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = D_REG_N;
	      *reg = 10 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '8' && ! ((src[2] - '0') & 1)
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = D_REG_N;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }
  if (l0 == 'x' && l1 == 'd')
    {
      if (src[2] == '1')
	{
	  if (src[3] >= '0' && src[3] <= '4' && ! ((src[3] - '0') & 1)
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = X_REG_N;
	      *reg = 11 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '8' && ! ((src[2] - '0') & 1)
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = X_REG_N;
	  *reg = (src[2] - '0') + 1;
	  return 3;
	}
    }
  if (l0 == 'f' && l1 == 'v')
    {
      if (src[2] == '1'&& src[3] == '2' && ! IDENT_CHAR ((unsigned char) src[4]))
	{
	  *mode = V_REG_N;
	  *reg = 12;
	  return 4;
	}
      if ((src[2] == '0' || src[2] == '4' || src[2] == '8')
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = V_REG_N;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }
  if (l0 == 'f' && l1 == 'p' && TOLOWER (src[2]) == 'u'
      && TOLOWER (src[3]) == 'l'
      && ! IDENT_CHAR ((unsigned char) src[4]))
    {
      *mode = FPUL_N;
      return 4;
    }

  if (l0 == 'f' && l1 == 'p' && TOLOWER (src[2]) == 's'
      && TOLOWER (src[3]) == 'c'
      && TOLOWER (src[4]) == 'r' && ! IDENT_CHAR ((unsigned char) src[5]))
    {
      *mode = FPSCR_N;
      return 5;
    }

  if (l0 == 'x' && l1 == 'm' && TOLOWER (src[2]) == 't'
      && TOLOWER (src[3]) == 'r'
      && TOLOWER (src[4]) == 'x' && ! IDENT_CHAR ((unsigned char) src[5]))
    {
      *mode = XMTRX_M4;
      return 5;
    }

  return 0;
}

static char *
parse_exp (char *s, sh_operand_info *op)
{
  char *save;
  char *new;

  save = input_line_pointer;
  input_line_pointer = s;
  expression (&op->immediate);
  if (op->immediate.X_op == O_absent)
    as_bad (_("missing operand"));
#ifdef OBJ_ELF
  else if (op->immediate.X_op == O_PIC_reloc
	   || sh_PIC_related_p (op->immediate.X_add_symbol)
	   || sh_PIC_related_p (op->immediate.X_op_symbol))
    as_bad (_("misplaced PIC operand"));
#endif
  new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

/* The many forms of operand:

   Rn                   Register direct
   @Rn                  Register indirect
   @Rn+                 Autoincrement
   @-Rn                 Autodecrement
   @(disp:4,Rn)
   @(disp:8,GBR)
   @(disp:8,PC)

   @(R0,Rn)
   @(R0,GBR)

   disp:8
   disp:12
   #imm8
   pr, gbr, vbr, macl, mach
 */

static char *
parse_at (char *src, sh_operand_info *op)
{
  int len;
  int mode;
  src++;
  if (src[0] == '-')
    {
      /* Must be predecrement.  */
      src++;

      len = parse_reg (src, &mode, &(op->reg));
      if (mode != A_REG_N)
	as_bad (_("illegal register after @-"));

      op->type = A_DEC_N;
      src += len;
    }
  else if (src[0] == '(')
    {
      /* Could be @(disp, rn), @(disp, gbr), @(disp, pc),  @(r0, gbr) or
         @(r0, rn).  */
      src++;
      len = parse_reg (src, &mode, &(op->reg));
      if (len && mode == A_REG_N)
	{
	  src += len;
	  if (op->reg != 0)
	    {
	      as_bad (_("must be @(r0,...)"));
	    }
	  if (src[0] == ',')
	    {
	      src++;
	      /* Now can be rn or gbr.  */
	      len = parse_reg (src, &mode, &(op->reg));
	    }
	  else
	    {
	      len = 0;
	    }
	  if (len)
	    {
	      if (mode == A_GBR)
		{
		  op->type = A_R0_GBR;
		}
	      else if (mode == A_REG_N)
		{
		  op->type = A_IND_R0_REG_N;
		}
	      else
		{
		  as_bad (_("syntax error in @(r0,...)"));
		}
	    }
	  else
	    {
	      as_bad (_("syntax error in @(r0...)"));
	    }
	}
      else
	{
	  /* Must be an @(disp,.. thing).  */
	  src = parse_exp (src, op);
	  if (src[0] == ',')
	    src++;
	  /* Now can be rn, gbr or pc.  */
	  len = parse_reg (src, &mode, &op->reg);
	  if (len)
	    {
	      if (mode == A_REG_N)
		{
		  op->type = A_DISP_REG_N;
		}
	      else if (mode == A_GBR)
		{
		  op->type = A_DISP_GBR;
		}
	      else if (mode == A_PC)
		{
		  /* We want @(expr, pc) to uniformly address . + expr,
		     no matter if expr is a constant, or a more complex
		     expression, e.g. sym-. or sym1-sym2.
		     However, we also used to accept @(sym,pc)
		     as addressing sym, i.e. meaning the same as plain sym.
		     Some existing code does use the @(sym,pc) syntax, so
		     we give it the old semantics for now, but warn about
		     its use, so that users have some time to fix their code.

		     Note that due to this backward compatibility hack,
		     we'll get unexpected results when @(offset, pc) is used,
		     and offset is a symbol that is set later to an an address
		     difference, or an external symbol that is set to an
		     address difference in another source file, so we want to
		     eventually remove it.  */
		  if (op->immediate.X_op == O_symbol)
		    {
		      op->type = A_DISP_PC;
		      as_warn (_("Deprecated syntax."));
		    }
		  else
		    {
		      op->type = A_DISP_PC_ABS;
		      /* Such operands don't get corrected for PC==.+4, so
			 make the correction here.  */
		      op->immediate.X_add_number -= 4;
		    }
		}
	      else
		{
		  as_bad (_("syntax error in @(disp,[Rn, gbr, pc])"));
		}
	    }
	  else
	    {
	      as_bad (_("syntax error in @(disp,[Rn, gbr, pc])"));
	    }
	}
      src += len;
      if (src[0] != ')')
	as_bad (_("expecting )"));
      else
	src++;
    }
  else
    {
      src += parse_reg (src, &mode, &(op->reg));
      if (mode != A_REG_N)
	as_bad (_("illegal register after @"));

      if (src[0] == '+')
	{
	  char l0, l1;

	  src++;
	  l0 = TOLOWER (src[0]);
	  l1 = TOLOWER (src[1]);

	  if ((l0 == 'r' && l1 == '8')
	      || (l0 == 'i' && (l1 == 'x' || l1 == 's')))
	    {
	      src += 2;
	      op->type = AX_PMOD_N;
	    }
	  else if (   (l0 == 'r' && l1 == '9')
		   || (l0 == 'i' && l1 == 'y'))
	    {
	      src += 2;
	      op->type = AY_PMOD_N;
	    }
	  else
	    op->type = A_INC_N;
	}
      else
	op->type = A_IND_N;
    }
  return src;
}

static void
get_operand (char **ptr, sh_operand_info *op)
{
  char *src = *ptr;
  int mode = -1;
  unsigned int len;

  if (src[0] == '#')
    {
      src++;
      *ptr = parse_exp (src, op);
      op->type = A_IMM;
      return;
    }

  else if (src[0] == '@')
    {
      *ptr = parse_at (src, op);
      return;
    }
  len = parse_reg (src, &mode, &(op->reg));
  if (len)
    {
      *ptr = src + len;
      op->type = mode;
      return;
    }
  else
    {
      /* Not a reg, the only thing left is a displacement.  */
      *ptr = parse_exp (src, op);
      op->type = A_DISP_PC;
      return;
    }
}

static char *
get_operands (sh_opcode_info *info, char *args, sh_operand_info *operand)
{
  char *ptr = args;
  if (info->arg[0])
    {
      /* The pre-processor will eliminate whitespace in front of '@'
	 after the first argument; we may be called multiple times
	 from assemble_ppi, so don't insist on finding whitespace here.  */
      if (*ptr == ' ')
	ptr++;

      get_operand (&ptr, operand + 0);
      if (info->arg[1])
	{
	  if (*ptr == ',')
	    {
	      ptr++;
	    }
	  get_operand (&ptr, operand + 1);
	  /* ??? Hack: psha/pshl have a varying operand number depending on
	     the type of the first operand.  We handle this by having the
	     three-operand version first and reducing the number of operands
	     parsed to two if we see that the first operand is an immediate.
             This works because no insn with three operands has an immediate
	     as first operand.  */
	  if (info->arg[2] && operand[0].type != A_IMM)
	    {
	      if (*ptr == ',')
		{
		  ptr++;
		}
	      get_operand (&ptr, operand + 2);
	    }
	  else
	    {
	      operand[2].type = 0;
	    }
	}
      else
	{
	  operand[1].type = 0;
	  operand[2].type = 0;
	}
    }
  else
    {
      operand[0].type = 0;
      operand[1].type = 0;
      operand[2].type = 0;
    }
  return ptr;
}

/* Passed a pointer to a list of opcodes which use different
   addressing modes, return the opcode which matches the opcodes
   provided.  */

static sh_opcode_info *
get_specific (sh_opcode_info *opcode, sh_operand_info *operands)
{
  sh_opcode_info *this_try = opcode;
  char *name = opcode->name;
  int n = 0;

  while (opcode->name)
    {
      this_try = opcode++;
      if ((this_try->name != name) && (strcmp (this_try->name, name) != 0))
	{
	  /* We've looked so far down the table that we've run out of
	     opcodes with the same name.  */
	  return 0;
	}

      /* Look at both operands needed by the opcodes and provided by
         the user - since an arg test will often fail on the same arg
         again and again, we'll try and test the last failing arg the
         first on each opcode try.  */
      for (n = 0; this_try->arg[n]; n++)
	{
	  sh_operand_info *user = operands + n;
	  sh_arg_type arg = this_try->arg[n];

	  switch (arg)
	    {
	    case A_DISP_PC:
	      if (user->type == A_DISP_PC_ABS)
		break;
	      /* Fall through.  */
	    case A_IMM:
	    case A_BDISP12:
	    case A_BDISP8:
	    case A_DISP_GBR:
	    case A_MACH:
	    case A_PR:
	    case A_MACL:
	      if (user->type != arg)
		goto fail;
	      break;
	    case A_R0:
	      /* opcode needs r0 */
	      if (user->type != A_REG_N || user->reg != 0)
		goto fail;
	      break;
	    case A_R0_GBR:
	      if (user->type != A_R0_GBR || user->reg != 0)
		goto fail;
	      break;
	    case F_FR0:
	      if (user->type != F_REG_N || user->reg != 0)
		goto fail;
	      break;

	    case A_REG_N:
	    case A_INC_N:
	    case A_DEC_N:
	    case A_IND_N:
	    case A_IND_R0_REG_N:
	    case A_DISP_REG_N:
	    case F_REG_N:
	    case D_REG_N:
	    case X_REG_N:
	    case V_REG_N:
	    case FPUL_N:
	    case FPSCR_N:
	    case DSP_REG_N:
	      /* Opcode needs rn */
	      if (user->type != arg)
		goto fail;
	      reg_n = user->reg;
	      break;
	    case DX_REG_N:
	      if (user->type != D_REG_N && user->type != X_REG_N)
		goto fail;
	      reg_n = user->reg;
	      break;
	    case A_GBR:
	    case A_SR:
	    case A_VBR:
	    case A_DSR:
	    case A_MOD:
	    case A_RE:
	    case A_RS:
	    case A_SSR:
	    case A_SPC:
	    case A_SGR:
	    case A_DBR:
	      if (user->type != arg)
		goto fail;
	      break;

	    case A_REG_B:
	      if (user->type != arg)
		goto fail;
	      reg_b = user->reg;
	      break;

	    case A_REG_M:
	    case A_INC_M:
	    case A_DEC_M:
	    case A_IND_M:
	    case A_IND_R0_REG_M:
	    case A_DISP_REG_M:
	    case DSP_REG_M:
	      /* Opcode needs rn */
	      if (user->type != arg - A_REG_M + A_REG_N)
		goto fail;
	      reg_m = user->reg;
	      break;

	    case AS_DEC_N:
	      if (user->type != A_DEC_N)
		goto fail;
	      if (user->reg < 2 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AS_INC_N:
	      if (user->type != A_INC_N)
		goto fail;
	      if (user->reg < 2 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AS_IND_N:
	      if (user->type != A_IND_N)
		goto fail;
	      if (user->reg < 2 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AS_PMOD_N:
	      if (user->type != AX_PMOD_N)
		goto fail;
	      if (user->reg < 2 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AX_INC_N:
	      if (user->type != A_INC_N)
		goto fail;
	      if (user->reg < 4 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AX_IND_N:
	      if (user->type != A_IND_N)
		goto fail;
	      if (user->reg < 4 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AX_PMOD_N:
	      if (user->type != AX_PMOD_N)
		goto fail;
	      if (user->reg < 4 || user->reg > 5)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AXY_INC_N:
	      if (user->type != A_INC_N)
		goto fail;
	      if ((user->reg < 4 || user->reg > 5)
		  && (user->reg < 0 || user->reg > 1))
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AXY_IND_N:
	      if (user->type != A_IND_N)
		goto fail;
	      if ((user->reg < 4 || user->reg > 5)
		  && (user->reg < 0 || user->reg > 1))
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AXY_PMOD_N:
	      if (user->type != AX_PMOD_N)
		goto fail;
	      if ((user->reg < 4 || user->reg > 5)
		  && (user->reg < 0 || user->reg > 1))
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AY_INC_N:
	      if (user->type != A_INC_N)
		goto fail;
	      if (user->reg < 6 || user->reg > 7)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AY_IND_N:
	      if (user->type != A_IND_N)
		goto fail;
	      if (user->reg < 6 || user->reg > 7)
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AY_PMOD_N:
	      if (user->type != AY_PMOD_N)
		goto fail;
	      if (user->reg < 6 || user->reg > 7)
		goto fail;
	      reg_n = user->reg;
	      break;

	    case AYX_INC_N:
	      if (user->type != A_INC_N)
		goto fail;
	      if ((user->reg < 6 || user->reg > 7)
		  && (user->reg < 2 || user->reg > 3))
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AYX_IND_N:
	      if (user->type != A_IND_N)
		goto fail;
	      if ((user->reg < 6 || user->reg > 7)
		  && (user->reg < 2 || user->reg > 3))
		goto fail;
	      reg_n = user->reg;
	      break;
	      
	    case AYX_PMOD_N:
	      if (user->type != AY_PMOD_N)
		goto fail;
	      if ((user->reg < 6 || user->reg > 7)
		  && (user->reg < 2 || user->reg > 3))
		goto fail;
	      reg_n = user->reg;
	      break;

	    case DSP_REG_A_M:
	      if (user->type != DSP_REG_N)
		goto fail;
	      if (user->reg != A_A0_NUM
		  && user->reg != A_A1_NUM)
		goto fail;
	      reg_m = user->reg;
	      break;

	    case DSP_REG_AX:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_A0_NUM:
		  reg_x = 0;
		  break;
		case A_A1_NUM:
		  reg_x = 2;
		  break;
		case A_X0_NUM:
		  reg_x = 1;
		  break;
		case A_X1_NUM:
		  reg_x = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_XY:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_X0_NUM:
		  reg_x = 0;
		  break;
		case A_X1_NUM:
		  reg_x = 2;
		  break;
		case A_Y0_NUM:
		  reg_x = 1;
		  break;
		case A_Y1_NUM:
		  reg_x = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_AY:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_A0_NUM:
		  reg_y = 0;
		  break;
		case A_A1_NUM:
		  reg_y = 1;
		  break;
		case A_Y0_NUM:
		  reg_y = 2;
		  break;
		case A_Y1_NUM:
		  reg_y = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_YX:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_Y0_NUM:
		  reg_y = 0;
		  break;
		case A_Y1_NUM:
		  reg_y = 1;
		  break;
		case A_X0_NUM:
		  reg_y = 2;
		  break;
		case A_X1_NUM:
		  reg_y = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_X:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_X0_NUM:
		  reg_x = 0;
		  break;
		case A_X1_NUM:
		  reg_x = 1;
		  break;
		case A_A0_NUM:
		  reg_x = 2;
		  break;
		case A_A1_NUM:
		  reg_x = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_Y:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_Y0_NUM:
		  reg_y = 0;
		  break;
		case A_Y1_NUM:
		  reg_y = 1;
		  break;
		case A_M0_NUM:
		  reg_y = 2;
		  break;
		case A_M1_NUM:
		  reg_y = 3;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_E:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_X0_NUM:
		  reg_efg = 0 << 10;
		  break;
		case A_X1_NUM:
		  reg_efg = 1 << 10;
		  break;
		case A_Y0_NUM:
		  reg_efg = 2 << 10;
		  break;
		case A_A1_NUM:
		  reg_efg = 3 << 10;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_F:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_Y0_NUM:
		  reg_efg |= 0 << 8;
		  break;
		case A_Y1_NUM:
		  reg_efg |= 1 << 8;
		  break;
		case A_X0_NUM:
		  reg_efg |= 2 << 8;
		  break;
		case A_A1_NUM:
		  reg_efg |= 3 << 8;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case DSP_REG_G:
	      if (user->type != DSP_REG_N)
		goto fail;
	      switch (user->reg)
		{
		case A_M0_NUM:
		  reg_efg |= 0 << 2;
		  break;
		case A_M1_NUM:
		  reg_efg |= 1 << 2;
		  break;
		case A_A0_NUM:
		  reg_efg |= 2 << 2;
		  break;
		case A_A1_NUM:
		  reg_efg |= 3 << 2;
		  break;
		default:
		  goto fail;
		}
	      break;

	    case A_A0:
	      if (user->type != DSP_REG_N || user->reg != A_A0_NUM)
		goto fail;
	      break;
	    case A_X0:
	      if (user->type != DSP_REG_N || user->reg != A_X0_NUM)
		goto fail;
	      break;
	    case A_X1:
	      if (user->type != DSP_REG_N || user->reg != A_X1_NUM)
		goto fail;
	      break;
	    case A_Y0:
	      if (user->type != DSP_REG_N || user->reg != A_Y0_NUM)
		goto fail;
	      break;
	    case A_Y1:
	      if (user->type != DSP_REG_N || user->reg != A_Y1_NUM)
		goto fail;
	      break;

	    case F_REG_M:
	    case D_REG_M:
	    case X_REG_M:
	    case V_REG_M:
	    case FPUL_M:
	    case FPSCR_M:
	      /* Opcode needs rn */
	      if (user->type != arg - F_REG_M + F_REG_N)
		goto fail;
	      reg_m = user->reg;
	      break;
	    case DX_REG_M:
	      if (user->type != D_REG_N && user->type != X_REG_N)
		goto fail;
	      reg_m = user->reg;
	      break;
	    case XMTRX_M4:
	      if (user->type != XMTRX_M4)
		goto fail;
	      reg_m = 4;
	      break;

	    default:
	      printf (_("unhandled %d\n"), arg);
	      goto fail;
	    }
	}
      if ( !(valid_arch & this_try->arch))
	goto fail;
      valid_arch &= this_try->arch;
      return this_try;
    fail:
      ;
    }

  return 0;
}

static void
insert (char *where, int how, int pcrel, sh_operand_info *op)
{
  fix_new_exp (frag_now,
	       where - frag_now->fr_literal,
	       2,
	       &op->immediate,
	       pcrel,
	       how);
}

static void
build_relax (sh_opcode_info *opcode, sh_operand_info *op)
{
  int high_byte = target_big_endian ? 0 : 1;
  char *p;

  if (opcode->arg[0] == A_BDISP8)
    {
      int what = (opcode->nibbles[1] & 4) ? COND_JUMP_DELAY : COND_JUMP;
      p = frag_var (rs_machine_dependent,
		    md_relax_table[C (what, COND32)].rlx_length,
		    md_relax_table[C (what, COND8)].rlx_length,
		    C (what, 0),
		    op->immediate.X_add_symbol,
		    op->immediate.X_add_number,
		    0);
      p[high_byte] = (opcode->nibbles[0] << 4) | (opcode->nibbles[1]);
    }
  else if (opcode->arg[0] == A_BDISP12)
    {
      p = frag_var (rs_machine_dependent,
		    md_relax_table[C (UNCOND_JUMP, UNCOND32)].rlx_length,
		    md_relax_table[C (UNCOND_JUMP, UNCOND12)].rlx_length,
		    C (UNCOND_JUMP, 0),
		    op->immediate.X_add_symbol,
		    op->immediate.X_add_number,
		    0);
      p[high_byte] = (opcode->nibbles[0] << 4);
    }

}

/* Insert ldrs & ldre with fancy relocations that relaxation can recognize.  */

static char *
insert_loop_bounds (char *output, sh_operand_info *operand)
{
  char *name;
  symbolS *end_sym;

  /* Since the low byte of the opcode will be overwritten by the reloc, we
     can just stash the high byte into both bytes and ignore endianness.  */
  output[0] = 0x8c;
  output[1] = 0x8c;
  insert (output, BFD_RELOC_SH_LOOP_START, 1, operand);
  insert (output, BFD_RELOC_SH_LOOP_END, 1, operand + 1);

  if (sh_relax)
    {
      static int count = 0;

      /* If the last loop insn is a two-byte-insn, it is in danger of being
	 swapped with the insn after it.  To prevent this, create a new
	 symbol - complete with SH_LABEL reloc - after the last loop insn.
	 If the last loop insn is four bytes long, the symbol will be
	 right in the middle, but four byte insns are not swapped anyways.  */
      /* A REPEAT takes 6 bytes.  The SH has a 32 bit address space.
	 Hence a 9 digit number should be enough to count all REPEATs.  */
      name = alloca (11);
      sprintf (name, "_R%x", count++ & 0x3fffffff);
      end_sym = symbol_new (name, undefined_section, 0, &zero_address_frag);
      /* Make this a local symbol.  */
#ifdef OBJ_COFF
      SF_SET_LOCAL (end_sym);
#endif /* OBJ_COFF */
      symbol_table_insert (end_sym);
      end_sym->sy_value = operand[1].immediate;
      end_sym->sy_value.X_add_number += 2;
      fix_new (frag_now, frag_now_fix (), 2, end_sym, 0, 1, BFD_RELOC_SH_LABEL);
    }

  output = frag_more (2);
  output[0] = 0x8e;
  output[1] = 0x8e;
  insert (output, BFD_RELOC_SH_LOOP_START, 1, operand);
  insert (output, BFD_RELOC_SH_LOOP_END, 1, operand + 1);

  return frag_more (2);
}

/* Now we know what sort of opcodes it is, let's build the bytes.  */

static unsigned int
build_Mytes (sh_opcode_info *opcode, sh_operand_info *operand)
{
  int index;
  char nbuf[4];
  char *output = frag_more (2);
  unsigned int size = 2;
  int low_byte = target_big_endian ? 1 : 0;
  nbuf[0] = 0;
  nbuf[1] = 0;
  nbuf[2] = 0;
  nbuf[3] = 0;

  for (index = 0; index < 4; index++)
    {
      sh_nibble_type i = opcode->nibbles[index];
      if (i < 16)
	{
	  nbuf[index] = i;
	}
      else
	{
	  switch (i)
	    {
	    case REG_N:
	    case REG_N_D:
	      nbuf[index] = reg_n;
	      break;
	    case REG_M:
	      nbuf[index] = reg_m;
	      break;
	    case SDT_REG_N:
	      if (reg_n < 2 || reg_n > 5)
		as_bad (_("Invalid register: 'r%d'"), reg_n);
	      nbuf[index] = (reg_n & 3) | 4;
	      break;
	    case REG_NM:
	      nbuf[index] = reg_n | (reg_m >> 2);
	      break;
	    case REG_B:
	      nbuf[index] = reg_b | 0x08;
	      break;
	    case REG_N_B01:
	      nbuf[index] = reg_n | 0x01;
	      break;
	    case IMM0_4BY4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4BY4, 0, operand);
	      break;
	    case IMM0_4BY2:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4BY2, 0, operand);
	      break;
	    case IMM0_4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4, 0, operand);
	      break;
	    case IMM1_4BY4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4BY4, 0, operand + 1);
	      break;
	    case IMM1_4BY2:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4BY2, 0, operand + 1);
	      break;
	    case IMM1_4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM4, 0, operand + 1);
	      break;
	    case IMM0_8BY4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8BY4, 0, operand);
	      break;
	    case IMM0_8BY2:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8BY2, 0, operand);
	      break;
	    case IMM0_8:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8, 0, operand);
	      break;
	    case IMM1_8BY4:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8BY4, 0, operand + 1);
	      break;
	    case IMM1_8BY2:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8BY2, 0, operand + 1);
	      break;
	    case IMM1_8:
	      insert (output + low_byte, BFD_RELOC_SH_IMM8, 0, operand + 1);
	      break;
	    case PCRELIMM_8BY4:
	      insert (output, BFD_RELOC_SH_PCRELIMM8BY4,
		      operand->type != A_DISP_PC_ABS, operand);
	      break;
	    case PCRELIMM_8BY2:
	      insert (output, BFD_RELOC_SH_PCRELIMM8BY2,
		      operand->type != A_DISP_PC_ABS, operand);
	      break;
	    case REPEAT:
	      output = insert_loop_bounds (output, operand);
	      nbuf[index] = opcode->nibbles[3];
	      operand += 2;
	      break;
	    default:
	      printf (_("failed for %d\n"), i);
	    }
	}
    }
  if (!target_big_endian)
    {
      output[1] = (nbuf[0] << 4) | (nbuf[1]);
      output[0] = (nbuf[2] << 4) | (nbuf[3]);
    }
  else
    {
      output[0] = (nbuf[0] << 4) | (nbuf[1]);
      output[1] = (nbuf[2] << 4) | (nbuf[3]);
    }
  return size;
}

/* Find an opcode at the start of *STR_P in the hash table, and set
   *STR_P to the first character after the last one read.  */

static sh_opcode_info *
find_cooked_opcode (char **str_p)
{
  char *str = *str_p;
  unsigned char *op_start;
  unsigned char *op_end;
  char name[20];
  int nlen = 0;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.
     The pre-processor will eliminate whitespace in front of
     any '@' after the first argument; we may be called from
     assemble_ppi, so the opcode might be terminated by an '@'.  */
  for (op_start = op_end = (unsigned char *) (str);
       *op_end
       && nlen < 20
       && !is_end_of_line[*op_end] && *op_end != ' ' && *op_end != '@';
       op_end++)
    {
      unsigned char c = op_start[nlen];

      /* The machine independent code will convert CMP/EQ into cmp/EQ
	 because it thinks the '/' is the end of the symbol.  Moreover,
	 all but the first sub-insn is a parallel processing insn won't
	 be capitalized.  Instead of hacking up the machine independent
	 code, we just deal with it here.  */
      c = TOLOWER (c);
      name[nlen] = c;
      nlen++;
    }

  name[nlen] = 0;
  *str_p = op_end;

  if (nlen == 0)
    as_bad (_("can't find opcode "));

  return (sh_opcode_info *) hash_find (opcode_hash_control, name);
}

/* Assemble a parallel processing insn.  */
#define DDT_BASE 0xf000 /* Base value for double data transfer insns */

static unsigned int
assemble_ppi (char *op_end, sh_opcode_info *opcode)
{
  int movx = 0;
  int movy = 0;
  int cond = 0;
  int field_b = 0;
  char *output;
  int move_code;
  unsigned int size;

  for (;;)
    {
      sh_operand_info operand[3];

      /* Some insn ignore one or more register fields, e.g. psts machl,a0.
	 Make sure we encode a defined insn pattern.  */
      reg_x = 0;
      reg_y = 0;
      reg_n = 0;

      if (opcode->arg[0] != A_END)
	op_end = get_operands (opcode, op_end, operand);
    try_another_opcode:
      opcode = get_specific (opcode, operand);
      if (opcode == 0)
	{
	  /* Couldn't find an opcode which matched the operands.  */
	  char *where = frag_more (2);
	  size = 2;

	  where[0] = 0x0;
	  where[1] = 0x0;
	  as_bad (_("invalid operands for opcode"));
	  return size;
	}

      if (opcode->nibbles[0] != PPI)
	as_bad (_("insn can't be combined with parallel processing insn"));

      switch (opcode->nibbles[1])
	{

	case NOPX:
	  if (movx)
	    as_bad (_("multiple movx specifications"));
	  movx = DDT_BASE;
	  break;
	case NOPY:
	  if (movy)
	    as_bad (_("multiple movy specifications"));
	  movy = DDT_BASE;
	  break;

	case MOVX_NOPY:
	  if (movx)
	    as_bad (_("multiple movx specifications"));
	  if ((reg_n < 4 || reg_n > 5)
	      && (reg_n < 0 || reg_n > 1))
	    as_bad (_("invalid movx address register"));
	  if (movy && movy != DDT_BASE)
	    as_bad (_("insn cannot be combined with non-nopy"));
	  movx = ((((reg_n & 1) != 0) << 9)
		  + (((reg_n & 4) == 0) << 8)
		  + (reg_x << 6)
		  + (opcode->nibbles[2] << 4)
		  + opcode->nibbles[3]
		  + DDT_BASE);
	  break;

	case MOVY_NOPX:
	  if (movy)
	    as_bad (_("multiple movy specifications"));
	  if ((reg_n < 6 || reg_n > 7)
	      && (reg_n < 2 || reg_n > 3))
	    as_bad (_("invalid movy address register"));
	  if (movx && movx != DDT_BASE)
	    as_bad (_("insn cannot be combined with non-nopx"));
	  movy = ((((reg_n & 1) != 0) << 8)
		  + (((reg_n & 4) == 0) << 9)
		  + (reg_y << 6)
		  + (opcode->nibbles[2] << 4)
		  + opcode->nibbles[3]
		  + DDT_BASE);
	  break;

	case MOVX:
	  if (movx)
	    as_bad (_("multiple movx specifications"));
	  if (movy & 0x2ac)
	    as_bad (_("previous movy requires nopx"));
	  if (reg_n < 4 || reg_n > 5)
	    as_bad (_("invalid movx address register"));
	  if (opcode->nibbles[2] & 8)
	    {
	      if (reg_m == A_A1_NUM)
		movx = 1 << 7;
	      else if (reg_m != A_A0_NUM)
		as_bad (_("invalid movx dsp register"));
	    }
	  else
	    {
	      if (reg_x > 1)
		as_bad (_("invalid movx dsp register"));
	      movx = reg_x << 7;
	    }
	  movx += ((reg_n - 4) << 9) + (opcode->nibbles[2] << 2) + DDT_BASE;
	  break;

	case MOVY:
	  if (movy)
	    as_bad (_("multiple movy specifications"));
	  if (movx & 0x153)
	    as_bad (_("previous movx requires nopy"));
	  if (opcode->nibbles[2] & 8)
	    {
	      /* Bit 3 in nibbles[2] is intended for bit 4 of the opcode,
		 so add 8 more.  */
	      movy = 8;
	      if (reg_m == A_A1_NUM)
		movy += 1 << 6;
	      else if (reg_m != A_A0_NUM)
		as_bad (_("invalid movy dsp register"));
	    }
	  else
	    {
	      if (reg_y > 1)
		as_bad (_("invalid movy dsp register"));
	      movy = reg_y << 6;
	    }
	  if (reg_n < 6 || reg_n > 7)
	    as_bad (_("invalid movy address register"));
	  movy += ((reg_n - 6) << 8) + opcode->nibbles[2] + DDT_BASE;
	  break;

	case PSH:
	  if (operand[0].immediate.X_op != O_constant)
	    as_bad (_("dsp immediate shift value not constant"));
	  field_b = ((opcode->nibbles[2] << 12)
		     | (operand[0].immediate.X_add_number & 127) << 4
		     | reg_n);
	  break;
	case PPI3NC:
	  if (cond)
	    {
	      opcode++;
	      goto try_another_opcode;
	    }
	  /* Fall through.  */
	case PPI3:
	  if (field_b)
	    as_bad (_("multiple parallel processing specifications"));
	  field_b = ((opcode->nibbles[2] << 12) + (opcode->nibbles[3] << 8)
		     + (reg_x << 6) + (reg_y << 4) + reg_n);
	  switch (opcode->nibbles[4])
	    {
	    case HEX_0:
	    case HEX_XX00:
	    case HEX_00YY:
	      break;
	    case HEX_1:
	    case HEX_4:
	      field_b += opcode->nibbles[4] << 4;
	      break;
	    default:
	      abort ();
	    }
	  break;
	case PDC:
	  if (cond)
	    as_bad (_("multiple condition specifications"));
	  cond = opcode->nibbles[2] << 8;
	  if (*op_end)
	    goto skip_cond_check;
	  break;
	case PPIC:
	  if (field_b)
	    as_bad (_("multiple parallel processing specifications"));
	  field_b = ((opcode->nibbles[2] << 12) + (opcode->nibbles[3] << 8)
		     + cond + (reg_x << 6) + (reg_y << 4) + reg_n);
	  cond = 0;
	  switch (opcode->nibbles[4])
	    {
	    case HEX_0:
	    case HEX_XX00:
	    case HEX_00YY:
	      break;
	    case HEX_1:
	    case HEX_4:
	      field_b += opcode->nibbles[4] << 4;
	      break;
	    default:
	      abort ();
	    }
	  break;
	case PMUL:
	  if (field_b)
	    {
	      if ((field_b & 0xef00) == 0xa100)
		field_b -= 0x8100;
	      /* pclr Dz pmuls Se,Sf,Dg */
	      else if ((field_b & 0xff00) == 0x8d00
		       && (valid_arch & arch_sh4al_dsp_up))
		{
		  valid_arch &= arch_sh4al_dsp_up;
		  field_b -= 0x8cf0;
		}
	      else
		as_bad (_("insn cannot be combined with pmuls"));
	      switch (field_b & 0xf)
		{
		case A_X0_NUM:
		  field_b += 0 - A_X0_NUM;
		  break;
		case A_Y0_NUM:
		  field_b += 1 - A_Y0_NUM;
		  break;
		case A_A0_NUM:
		  field_b += 2 - A_A0_NUM;
		  break;
		case A_A1_NUM:
		  field_b += 3 - A_A1_NUM;
		  break;
		default:
		  as_bad (_("bad combined pmuls output operand"));
		}
		/* Generate warning if the destination register for padd / psub
		   and pmuls is the same ( only for A0 or A1 ).
		   If the last nibble is 1010 then A0 is used in both
		   padd / psub and pmuls. If it is 1111 then A1 is used
		   as destination register in both padd / psub and pmuls.  */

		if ((((field_b | reg_efg) & 0x000F) == 0x000A)
		    || (((field_b | reg_efg) & 0x000F) == 0x000F))
		  as_warn (_("destination register is same for parallel insns"));
	    }
	  field_b += 0x4000 + reg_efg;
	  break;
	default:
	  abort ();
	}
      if (cond)
	{
	  as_bad (_("condition not followed by conditionalizable insn"));
	  cond = 0;
	}
      if (! *op_end)
	break;
    skip_cond_check:
      opcode = find_cooked_opcode (&op_end);
      if (opcode == NULL)
	{
	  (as_bad
	   (_("unrecognized characters at end of parallel processing insn")));
	  break;
	}
    }

  move_code = movx | movy;
  if (field_b)
    {
      /* Parallel processing insn.  */
      unsigned long ppi_code = (movx | movy | 0xf800) << 16 | field_b;

      output = frag_more (4);
      size = 4;
      if (! target_big_endian)
	{
	  output[3] = ppi_code >> 8;
	  output[2] = ppi_code;
	}
      else
	{
	  output[2] = ppi_code >> 8;
	  output[3] = ppi_code;
	}
      move_code |= 0xf800;
    }
  else
    {
      /* Just a double data transfer.  */
      output = frag_more (2);
      size = 2;
    }
  if (! target_big_endian)
    {
      output[1] = move_code >> 8;
      output[0] = move_code;
    }
  else
    {
      output[0] = move_code >> 8;
      output[1] = move_code;
    }
  return size;
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (char *str)
{
  unsigned char *op_end;
  sh_operand_info operand[3];
  sh_opcode_info *opcode;
  unsigned int size = 0;

#ifdef HAVE_SH64
  if (sh64_isa_mode == sh64_isa_shmedia)
    {
      shmedia_md_assemble (str);
      return;
    }
  else
    {
      /* If we've seen pseudo-directives, make sure any emitted data or
	 frags are marked as data.  */
      if (!seen_insn)
	{
	  sh64_update_contents_mark (TRUE);
	  sh64_set_contents_type (CRT_SH5_ISA16);
	}

      seen_insn = TRUE;
    }
#endif /* HAVE_SH64 */

  opcode = find_cooked_opcode (&str);
  op_end = str;

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode"));
      return;
    }

  if (sh_relax
      && ! seg_info (now_seg)->tc_segment_info_data.in_code)
    {
      /* Output a CODE reloc to tell the linker that the following
         bytes are instructions, not data.  */
      fix_new (frag_now, frag_now_fix (), 2, &abs_symbol, 0, 0,
	       BFD_RELOC_SH_CODE);
      seg_info (now_seg)->tc_segment_info_data.in_code = 1;
    }

  if (opcode->nibbles[0] == PPI)
    {
      size = assemble_ppi (op_end, opcode);
    }
  else
    {
      if (opcode->arg[0] == A_BDISP12
	  || opcode->arg[0] == A_BDISP8)
	{
	  /* Since we skip get_specific here, we have to check & update
	     valid_arch now.  */
	  if (valid_arch & opcode->arch)
	    valid_arch &= opcode->arch;
	  else
	    as_bad (_("Delayed branches not available on SH1"));
	  parse_exp (op_end + 1, &operand[0]);
	  build_relax (opcode, &operand[0]);
	}
      else
	{
	  if (opcode->arg[0] == A_END)
	    {
	      /* Ignore trailing whitespace.  If there is any, it has already
		 been compressed to a single space.  */
	      if (*op_end == ' ')
		op_end++;
	    }
	  else
	    {
	      op_end = get_operands (opcode, op_end, operand);
	    }
	  opcode = get_specific (opcode, operand);

	  if (opcode == 0)
	    {
	      /* Couldn't find an opcode which matched the operands.  */
	      char *where = frag_more (2);
	      size = 2;

	      where[0] = 0x0;
	      where[1] = 0x0;
	      as_bad (_("invalid operands for opcode"));
	    }
	  else
	    {
	      if (*op_end)
		as_bad (_("excess operands: '%s'"), op_end);

	      size = build_Mytes (opcode, operand);
	    }
	}
    }

#ifdef BFD_ASSEMBLER
  dwarf2_emit_insn (size);
#endif
}

/* This routine is called each time a label definition is seen.  It
   emits a BFD_RELOC_SH_LABEL reloc if necessary.  */

void
sh_frob_label (void)
{
  static fragS *last_label_frag;
  static int last_label_offset;

  if (sh_relax
      && seg_info (now_seg)->tc_segment_info_data.in_code)
    {
      int offset;

      offset = frag_now_fix ();
      if (frag_now != last_label_frag
	  || offset != last_label_offset)
	{
	  fix_new (frag_now, offset, 2, &abs_symbol, 0, 0, BFD_RELOC_SH_LABEL);
	  last_label_frag = frag_now;
	  last_label_offset = offset;
	}
    }
}

/* This routine is called when the assembler is about to output some
   data.  It emits a BFD_RELOC_SH_DATA reloc if necessary.  */

void
sh_flush_pending_output (void)
{
  if (sh_relax
      && seg_info (now_seg)->tc_segment_info_data.in_code)
    {
      fix_new (frag_now, frag_now_fix (), 2, &abs_symbol, 0, 0,
	       BFD_RELOC_SH_DATA);
      seg_info (now_seg)->tc_segment_info_data.in_code = 0;
    }
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

#ifdef OBJ_COFF
#ifndef BFD_ASSEMBLER

void
tc_crawl_symbol_chain (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_crawl_symbol_chain \n"));
}

void
tc_headers_hook (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_headers_hook \n"));
}

#endif
#endif

/* Various routines to kill one day.  */
/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP .  An error message is
   returned, or NULL on OK.  */

char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  if (! target_big_endian)
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }
  else
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }

  return NULL;
}

/* Handle the .uses pseudo-op.  This pseudo-op is used just before a
   call instruction.  It refers to a label of the instruction which
   loads the register which the call uses.  We use it to generate a
   special reloc for the linker.  */

static void
s_uses (int ignore ATTRIBUTE_UNUSED)
{
  expressionS ex;

  if (! sh_relax)
    as_warn (_(".uses pseudo-op seen when not relaxing"));

  expression (&ex);

  if (ex.X_op != O_symbol || ex.X_add_number != 0)
    {
      as_bad (_("bad .uses format"));
      ignore_rest_of_line ();
      return;
    }

  fix_new_exp (frag_now, frag_now_fix (), 2, &ex, 1, BFD_RELOC_SH_USES);

  demand_empty_rest_of_line ();
}

const char *md_shortopts = "";
struct option md_longopts[] =
{
#define OPTION_RELAX  (OPTION_MD_BASE)
#define OPTION_BIG (OPTION_MD_BASE + 1)
#define OPTION_LITTLE (OPTION_BIG + 1)
#define OPTION_SMALL (OPTION_LITTLE + 1)
#define OPTION_DSP (OPTION_SMALL + 1)
#define OPTION_ISA                    (OPTION_DSP + 1)
#define OPTION_RENESAS (OPTION_ISA + 1)

  {"relax", no_argument, NULL, OPTION_RELAX},
  {"big", no_argument, NULL, OPTION_BIG},
  {"little", no_argument, NULL, OPTION_LITTLE},
  {"small", no_argument, NULL, OPTION_SMALL},
  {"dsp", no_argument, NULL, OPTION_DSP},
  {"isa",                    required_argument, NULL, OPTION_ISA},
  {"renesas", no_argument, NULL, OPTION_RENESAS},

#ifdef HAVE_SH64
#define OPTION_ABI                    (OPTION_RENESAS + 1)
#define OPTION_NO_MIX                 (OPTION_ABI + 1)
#define OPTION_SHCOMPACT_CONST_CRANGE (OPTION_NO_MIX + 1)
#define OPTION_NO_EXPAND              (OPTION_SHCOMPACT_CONST_CRANGE + 1)
#define OPTION_PT32                   (OPTION_NO_EXPAND + 1)
  {"abi",                    required_argument, NULL, OPTION_ABI},
  {"no-mix",                 no_argument, NULL, OPTION_NO_MIX},
  {"shcompact-const-crange", no_argument, NULL, OPTION_SHCOMPACT_CONST_CRANGE},
  {"no-expand",              no_argument, NULL, OPTION_NO_EXPAND},
  {"expand-pt32",            no_argument, NULL, OPTION_PT32},
#endif /* HAVE_SH64 */

  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_RELAX:
      sh_relax = 1;
      break;

    case OPTION_BIG:
      target_big_endian = 1;
      break;

    case OPTION_LITTLE:
      target_big_endian = 0;
      break;

    case OPTION_SMALL:
      sh_small = 1;
      break;

    case OPTION_DSP:
      preset_target_arch = arch_sh1_up & ~arch_sh2e_up;
      break;

    case OPTION_RENESAS:
      dont_adjust_reloc_32 = 1;
      break;

    case OPTION_ISA:
      if (strcasecmp (arg, "sh4") == 0)
	preset_target_arch = arch_sh4;
      else if (strcasecmp (arg, "sh4a") == 0)
	preset_target_arch = arch_sh4a;
      else if (strcasecmp (arg, "dsp") == 0)
	preset_target_arch = arch_sh1_up & ~arch_sh2e_up;
      else if (strcasecmp (arg, "fp") == 0)
	preset_target_arch = arch_sh2e_up;
      else if (strcasecmp (arg, "any") == 0)
	preset_target_arch = arch_sh1_up;
#ifdef HAVE_SH64
      else if (strcasecmp (arg, "shmedia") == 0)
	{
	  if (sh64_isa_mode == sh64_isa_shcompact)
	    as_bad (_("Invalid combination: --isa=SHcompact with --isa=SHmedia"));
	  sh64_isa_mode = sh64_isa_shmedia;
	}
      else if (strcasecmp (arg, "shcompact") == 0)
	{
	  if (sh64_isa_mode == sh64_isa_shmedia)
	    as_bad (_("Invalid combination: --isa=SHmedia with --isa=SHcompact"));
	  if (sh64_abi == sh64_abi_64)
	    as_bad (_("Invalid combination: --abi=64 with --isa=SHcompact"));
	  sh64_isa_mode = sh64_isa_shcompact;
	}
#endif /* HAVE_SH64 */
      else
	as_bad ("Invalid argument to --isa option: %s", arg);
      break;

#ifdef HAVE_SH64
    case OPTION_ABI:
      if (strcmp (arg, "32") == 0)
	{
	  if (sh64_abi == sh64_abi_64)
	    as_bad (_("Invalid combination: --abi=32 with --abi=64"));
	  sh64_abi = sh64_abi_32;
	}
      else if (strcmp (arg, "64") == 0)
	{
	  if (sh64_abi == sh64_abi_32)
	    as_bad (_("Invalid combination: --abi=64 with --abi=32"));
	  if (sh64_isa_mode == sh64_isa_shcompact)
	    as_bad (_("Invalid combination: --isa=SHcompact with --abi=64"));
	  sh64_abi = sh64_abi_64;
	}
      else
	as_bad ("Invalid argument to --abi option: %s", arg);
      break;

    case OPTION_NO_MIX:
      sh64_mix = FALSE;
      break;

    case OPTION_SHCOMPACT_CONST_CRANGE:
      sh64_shcompact_const_crange = TRUE;
      break;

    case OPTION_NO_EXPAND:
      sh64_expand = FALSE;
      break;

    case OPTION_PT32:
      sh64_pt32 = TRUE;
      break;
#endif /* HAVE_SH64 */

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
SH options:\n\
-little			generate little endian code\n\
-big			generate big endian code\n\
-relax			alter jump instructions for long displacements\n\
-renesas		disable optimization with section symbol for\n\
			compatibility with Renesas assembler.\n\
-small			align sections to 4 byte boundaries, not 16\n\
-dsp			enable sh-dsp insns, and disable floating-point ISAs.\n"));
#ifdef HAVE_SH64
  fprintf (stream, _("\
-isa=[sh4\n\
    | sh4a\n\
    | dsp		same as '-dsp'\n\
    | fp\n\
    | shmedia		set as the default instruction set for SH64\n\
    | SHmedia\n\
    | shcompact\n\
    | SHcompact\n"));
  fprintf (stream, _("\
-abi=[32|64]		set size of expanded SHmedia operands and object\n\
			file type\n\
-shcompact-const-crange	emit code-range descriptors for constants in\n\
			SHcompact code sections\n\
-no-mix			disallow SHmedia code in the same section as\n\
			constants and SHcompact code\n\
-no-expand		do not expand MOVI, PT, PTA or PTB instructions\n\
-expand-pt32		with -abi=64, expand PT, PTA and PTB instructions\n\
			to 32 bits only\n"));
#else
  fprintf (stream, _("\
-isa=[sh4\n\
    | sh4a\n\
    | dsp		same as '-dsp'\n\
    | fp\n\
    | any]\n"));
#endif /* HAVE_SH64 */
}

/* This struct is used to pass arguments to sh_count_relocs through
   bfd_map_over_sections.  */

struct sh_count_relocs
{
  /* Symbol we are looking for.  */
  symbolS *sym;
  /* Count of relocs found.  */
  int count;
};

/* Count the number of fixups in a section which refer to a particular
   symbol.  When using BFD_ASSEMBLER, this is called via
   bfd_map_over_sections.  */

static void
sh_count_relocs (bfd *abfd ATTRIBUTE_UNUSED, segT sec, void *data)
{
  struct sh_count_relocs *info = (struct sh_count_relocs *) data;
  segment_info_type *seginfo;
  symbolS *sym;
  fixS *fix;

  seginfo = seg_info (sec);
  if (seginfo == NULL)
    return;

  sym = info->sym;
  for (fix = seginfo->fix_root; fix != NULL; fix = fix->fx_next)
    {
      if (fix->fx_addsy == sym)
	{
	  ++info->count;
	  fix->fx_tcbit = 1;
	}
    }
}

/* Handle the count relocs for a particular section.  When using
   BFD_ASSEMBLER, this is called via bfd_map_over_sections.  */

static void
sh_frob_section (bfd *abfd ATTRIBUTE_UNUSED, segT sec,
		 void *ignore ATTRIBUTE_UNUSED)
{
  segment_info_type *seginfo;
  fixS *fix;

  seginfo = seg_info (sec);
  if (seginfo == NULL)
    return;

  for (fix = seginfo->fix_root; fix != NULL; fix = fix->fx_next)
    {
      symbolS *sym;
      bfd_vma val;
      fixS *fscan;
      struct sh_count_relocs info;

      if (fix->fx_r_type != BFD_RELOC_SH_USES)
	continue;

      /* The BFD_RELOC_SH_USES reloc should refer to a defined local
	 symbol in the same section.  */
      sym = fix->fx_addsy;
      if (sym == NULL
	  || fix->fx_subsy != NULL
	  || fix->fx_addnumber != 0
	  || S_GET_SEGMENT (sym) != sec
#if ! defined (BFD_ASSEMBLER) && defined (OBJ_COFF)
	  || S_GET_STORAGE_CLASS (sym) == C_EXT
#endif
	  || S_IS_EXTERNAL (sym))
	{
	  as_warn_where (fix->fx_file, fix->fx_line,
			 _(".uses does not refer to a local symbol in the same section"));
	  continue;
	}

      /* Look through the fixups again, this time looking for one
	 at the same location as sym.  */
      val = S_GET_VALUE (sym);
      for (fscan = seginfo->fix_root;
	   fscan != NULL;
	   fscan = fscan->fx_next)
	if (val == fscan->fx_frag->fr_address + fscan->fx_where
	    && fscan->fx_r_type != BFD_RELOC_SH_ALIGN
	    && fscan->fx_r_type != BFD_RELOC_SH_CODE
	    && fscan->fx_r_type != BFD_RELOC_SH_DATA
	    && fscan->fx_r_type != BFD_RELOC_SH_LABEL)
	  break;
      if (fscan == NULL)
	{
	  as_warn_where (fix->fx_file, fix->fx_line,
			 _("can't find fixup pointed to by .uses"));
	  continue;
	}

      if (fscan->fx_tcbit)
	{
	  /* We've already done this one.  */
	  continue;
	}

      /* The variable fscan should also be a fixup to a local symbol
	 in the same section.  */
      sym = fscan->fx_addsy;
      if (sym == NULL
	  || fscan->fx_subsy != NULL
	  || fscan->fx_addnumber != 0
	  || S_GET_SEGMENT (sym) != sec
#if ! defined (BFD_ASSEMBLER) && defined (OBJ_COFF)
	  || S_GET_STORAGE_CLASS (sym) == C_EXT
#endif
	  || S_IS_EXTERNAL (sym))
	{
	  as_warn_where (fix->fx_file, fix->fx_line,
			 _(".uses target does not refer to a local symbol in the same section"));
	  continue;
	}

      /* Now we look through all the fixups of all the sections,
	 counting the number of times we find a reference to sym.  */
      info.sym = sym;
      info.count = 0;
#ifdef BFD_ASSEMBLER
      bfd_map_over_sections (stdoutput, sh_count_relocs, &info);
#else
      {
	int iscan;

	for (iscan = SEG_E0; iscan < SEG_UNKNOWN; iscan++)
	  sh_count_relocs ((bfd *) NULL, iscan, &info);
      }
#endif

      if (info.count < 1)
	abort ();

      /* Generate a BFD_RELOC_SH_COUNT fixup at the location of sym.
	 We have already adjusted the value of sym to include the
	 fragment address, so we undo that adjustment here.  */
      subseg_change (sec, 0);
      fix_new (fscan->fx_frag,
	       S_GET_VALUE (sym) - fscan->fx_frag->fr_address,
	       4, &abs_symbol, info.count, 0, BFD_RELOC_SH_COUNT);
    }
}

/* This function is called after the symbol table has been completed,
   but before the relocs or section contents have been written out.
   If we have seen any .uses pseudo-ops, they point to an instruction
   which loads a register with the address of a function.  We look
   through the fixups to find where the function address is being
   loaded from.  We then generate a COUNT reloc giving the number of
   times that function address is referred to.  The linker uses this
   information when doing relaxing, to decide when it can eliminate
   the stored function address entirely.  */

void
sh_frob_file (void)
{
#ifdef HAVE_SH64
  shmedia_frob_file_before_adjust ();
#endif

  if (! sh_relax)
    return;

#ifdef BFD_ASSEMBLER
  bfd_map_over_sections (stdoutput, sh_frob_section, NULL);
#else
  {
    int iseg;

    for (iseg = SEG_E0; iseg < SEG_UNKNOWN; iseg++)
      sh_frob_section ((bfd *) NULL, iseg, NULL);
  }
#endif
}

/* Called after relaxing.  Set the correct sizes of the fragments, and
   create relocs so that md_apply_fix3 will fill in the correct values.  */

void
#ifdef BFD_ASSEMBLER
md_convert_frag (bfd *headers ATTRIBUTE_UNUSED, segT seg, fragS *fragP)
#else
md_convert_frag (object_headers *headers ATTRIBUTE_UNUSED, segT seg,
		 fragS *fragP)
#endif
{
  int donerelax = 0;

  switch (fragP->fr_subtype)
    {
    case C (COND_JUMP, COND8):
    case C (COND_JUMP_DELAY, COND8):
      subseg_change (seg, 0);
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, BFD_RELOC_SH_PCDISP8BY2);
      fragP->fr_fix += 2;
      fragP->fr_var = 0;
      break;

    case C (UNCOND_JUMP, UNCOND12):
      subseg_change (seg, 0);
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, BFD_RELOC_SH_PCDISP12BY2);
      fragP->fr_fix += 2;
      fragP->fr_var = 0;
      break;

    case C (UNCOND_JUMP, UNCOND32):
    case C (UNCOND_JUMP, UNDEF_WORD_DISP):
      if (fragP->fr_symbol == NULL)
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement overflows 12-bit field"));
      else if (S_IS_DEFINED (fragP->fr_symbol))
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement to defined symbol %s overflows 12-bit field"),
		      S_GET_NAME (fragP->fr_symbol));
      else
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement to undefined symbol %s overflows 12-bit field"),
		      S_GET_NAME (fragP->fr_symbol));
      /* Stabilize this frag, so we don't trip an assert.  */
      fragP->fr_fix += fragP->fr_var;
      fragP->fr_var = 0;
      break;

    case C (COND_JUMP, COND12):
    case C (COND_JUMP_DELAY, COND12):
      /* A bcond won't fit, so turn it into a b!cond; bra disp; nop.  */
      /* I found that a relax failure for gcc.c-torture/execute/930628-1.c
	 was due to gas incorrectly relaxing an out-of-range conditional
	 branch with delay slot.  It turned:
                     bf.s    L6              (slot mov.l   r12,@(44,r0))
         into:

2c:  8f 01 a0 8b     bf.s    32 <_main+32>   (slot bra       L6)
30:  00 09           nop
32:  10 cb           mov.l   r12,@(44,r0)
         Therefore, branches with delay slots have to be handled
	 differently from ones without delay slots.  */
      {
	unsigned char *buffer =
	  (unsigned char *) (fragP->fr_fix + fragP->fr_literal);
	int highbyte = target_big_endian ? 0 : 1;
	int lowbyte = target_big_endian ? 1 : 0;
	int delay = fragP->fr_subtype == C (COND_JUMP_DELAY, COND12);

	/* Toggle the true/false bit of the bcond.  */
	buffer[highbyte] ^= 0x2;

	/* If this is a delayed branch, we may not put the bra in the
	   slot.  So we change it to a non-delayed branch, like that:
	   b! cond slot_label; bra disp; slot_label: slot_insn
	   ??? We should try if swapping the conditional branch and
	   its delay-slot insn already makes the branch reach.  */

	/* Build a relocation to six / four bytes farther on.  */
	subseg_change (seg, 0);
	fix_new (fragP, fragP->fr_fix, 2,
#ifdef BFD_ASSEMBLER
		 section_symbol (seg),
#else
		 seg_info (seg)->dot,
#endif
		 fragP->fr_address + fragP->fr_fix + (delay ? 4 : 6),
		 1, BFD_RELOC_SH_PCDISP8BY2);

	/* Set up a jump instruction.  */
	buffer[highbyte + 2] = 0xa0;
	buffer[lowbyte + 2] = 0;
	fix_new (fragP, fragP->fr_fix + 2, 2, fragP->fr_symbol,
		 fragP->fr_offset, 1, BFD_RELOC_SH_PCDISP12BY2);

	if (delay)
	  {
	    buffer[highbyte] &= ~0x4; /* Removes delay slot from branch.  */
	    fragP->fr_fix += 4;
	  }
	else
	  {
	    /* Fill in a NOP instruction.  */
	    buffer[highbyte + 4] = 0x0;
	    buffer[lowbyte + 4] = 0x9;

	    fragP->fr_fix += 6;
	  }
	fragP->fr_var = 0;
	donerelax = 1;
      }
      break;

    case C (COND_JUMP, COND32):
    case C (COND_JUMP_DELAY, COND32):
    case C (COND_JUMP, UNDEF_WORD_DISP):
    case C (COND_JUMP_DELAY, UNDEF_WORD_DISP):
      if (fragP->fr_symbol == NULL)
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement overflows 8-bit field"));
      else if (S_IS_DEFINED (fragP->fr_symbol))
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement to defined symbol %s overflows 8-bit field"),
		      S_GET_NAME (fragP->fr_symbol));
      else
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("displacement to undefined symbol %s overflows 8-bit field "),
		      S_GET_NAME (fragP->fr_symbol));
      /* Stabilize this frag, so we don't trip an assert.  */
      fragP->fr_fix += fragP->fr_var;
      fragP->fr_var = 0;
      break;

    default:
#ifdef HAVE_SH64
      shmedia_md_convert_frag (headers, seg, fragP, TRUE);
#else
      abort ();
#endif
    }

  if (donerelax && !sh_relax)
    as_warn_where (fragP->fr_file, fragP->fr_line,
		   _("overflow in branch to %s; converted into longer instruction sequence"),
		   (fragP->fr_symbol != NULL
		    ? S_GET_NAME (fragP->fr_symbol)
		    : ""));
}

valueT
md_section_align (segT seg ATTRIBUTE_UNUSED, valueT size)
{
#ifdef BFD_ASSEMBLER
#ifdef OBJ_ELF
  return size;
#else /* ! OBJ_ELF */
  return ((size + (1 << bfd_get_section_alignment (stdoutput, seg)) - 1)
	  & (-1 << bfd_get_section_alignment (stdoutput, seg)));
#endif /* ! OBJ_ELF */
#else /* ! BFD_ASSEMBLER */
  return ((size + (1 << section_alignment[(int) seg]) - 1)
	  & (-1 << section_alignment[(int) seg]));
#endif /* ! BFD_ASSEMBLER */
}

/* This static variable is set by s_uacons to tell sh_cons_align that
   the expression does not need to be aligned.  */

static int sh_no_align_cons = 0;

/* This handles the unaligned space allocation pseudo-ops, such as
   .uaword.  .uaword is just like .word, but the value does not need
   to be aligned.  */

static void
s_uacons (int bytes)
{
  /* Tell sh_cons_align not to align this value.  */
  sh_no_align_cons = 1;
  cons (bytes);
}

/* If a .word, et. al., pseud-op is seen, warn if the value is not
   aligned correctly.  Note that this can cause warnings to be issued
   when assembling initialized structured which were declared with the
   packed attribute.  FIXME: Perhaps we should require an option to
   enable this warning?  */

void
sh_cons_align (int nbytes)
{
  int nalign;
  char *p;

  if (sh_no_align_cons)
    {
      /* This is an unaligned pseudo-op.  */
      sh_no_align_cons = 0;
      return;
    }

  nalign = 0;
  while ((nbytes & 1) == 0)
    {
      ++nalign;
      nbytes >>= 1;
    }

  if (nalign == 0)
    return;

  if (now_seg == absolute_section)
    {
      if ((abs_section_offset & ((1 << nalign) - 1)) != 0)
	as_warn (_("misaligned data"));
      return;
    }

  p = frag_var (rs_align_test, 1, 1, (relax_substateT) 0,
		(symbolS *) NULL, (offsetT) nalign, (char *) NULL);

  record_alignment (now_seg, nalign);
}

/* When relaxing, we need to output a reloc for any .align directive
   that requests alignment to a four byte boundary or larger.  This is
   also where we check for misaligned data.  */

void
sh_handle_align (fragS *frag)
{
  int bytes = frag->fr_next->fr_address - frag->fr_address - frag->fr_fix;

  if (frag->fr_type == rs_align_code)
    {
      static const unsigned char big_nop_pattern[] = { 0x00, 0x09 };
      static const unsigned char little_nop_pattern[] = { 0x09, 0x00 };

      char *p = frag->fr_literal + frag->fr_fix;

      if (bytes & 1)
	{
	  *p++ = 0;
	  bytes--;
	  frag->fr_fix += 1;
	}

      if (target_big_endian)
	{
	  memcpy (p, big_nop_pattern, sizeof big_nop_pattern);
	  frag->fr_var = sizeof big_nop_pattern;
	}
      else
	{
	  memcpy (p, little_nop_pattern, sizeof little_nop_pattern);
	  frag->fr_var = sizeof little_nop_pattern;
	}
    }
  else if (frag->fr_type == rs_align_test)
    {
      if (bytes != 0)
	as_warn_where (frag->fr_file, frag->fr_line, _("misaligned data"));
    }

  if (sh_relax
      && (frag->fr_type == rs_align
	  || frag->fr_type == rs_align_code)
      && frag->fr_address + frag->fr_fix > 0
      && frag->fr_offset > 1
      && now_seg != bss_section)
    fix_new (frag, frag->fr_fix, 2, &abs_symbol, frag->fr_offset, 0,
	     BFD_RELOC_SH_ALIGN);
}

/* See whether the relocation should be resolved locally.  */

static bfd_boolean
sh_local_pcrel (fixS *fix)
{
  return (! sh_relax
	  && (fix->fx_r_type == BFD_RELOC_SH_PCDISP8BY2
	      || fix->fx_r_type == BFD_RELOC_SH_PCDISP12BY2
	      || fix->fx_r_type == BFD_RELOC_SH_PCRELIMM8BY2
	      || fix->fx_r_type == BFD_RELOC_SH_PCRELIMM8BY4
	      || fix->fx_r_type == BFD_RELOC_8_PCREL
	      || fix->fx_r_type == BFD_RELOC_SH_SWITCH16
	      || fix->fx_r_type == BFD_RELOC_SH_SWITCH32));
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
sh_force_relocation (fixS *fix)
{
  /* These relocations can't make it into a DSO, so no use forcing
     them for global symbols.  */
  if (sh_local_pcrel (fix))
    return 0;

  /* Make sure some relocations get emitted.  */
  if (fix->fx_r_type == BFD_RELOC_SH_LOOP_START
      || fix->fx_r_type == BFD_RELOC_SH_LOOP_END
      || fix->fx_r_type == BFD_RELOC_SH_TLS_GD_32
      || fix->fx_r_type == BFD_RELOC_SH_TLS_LD_32
      || fix->fx_r_type == BFD_RELOC_SH_TLS_IE_32
      || fix->fx_r_type == BFD_RELOC_SH_TLS_LDO_32
      || fix->fx_r_type == BFD_RELOC_SH_TLS_LE_32
      || generic_force_reloc (fix))
    return 1;

  if (! sh_relax)
    return 0;

  return (fix->fx_pcrel
	  || SWITCH_TABLE (fix)
	  || fix->fx_r_type == BFD_RELOC_SH_COUNT
	  || fix->fx_r_type == BFD_RELOC_SH_ALIGN
	  || fix->fx_r_type == BFD_RELOC_SH_CODE
	  || fix->fx_r_type == BFD_RELOC_SH_DATA
#ifdef HAVE_SH64
	  || fix->fx_r_type == BFD_RELOC_SH_SHMEDIA_CODE
#endif
	  || fix->fx_r_type == BFD_RELOC_SH_LABEL);
}

#ifdef OBJ_ELF
bfd_boolean
sh_fix_adjustable (fixS *fixP)
{
  if (fixP->fx_r_type == BFD_RELOC_32_PLT_PCREL
      || fixP->fx_r_type == BFD_RELOC_32_GOT_PCREL
      || fixP->fx_r_type == BFD_RELOC_SH_GOTPC
      || ((fixP->fx_r_type == BFD_RELOC_32) && dont_adjust_reloc_32)
      || fixP->fx_r_type == BFD_RELOC_RVA)
    return 0;

  /* We need the symbol name for the VTABLE entries */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

void
sh_elf_final_processing (void)
{
  int val;

  /* Set file-specific flags to indicate if this code needs
     a processor with the sh-dsp / sh2e ISA to execute.  */
#ifdef HAVE_SH64
  /* SH5 and above don't know about the valid_arch arch_sh* bits defined
     in sh-opc.h, so check SH64 mode before checking valid_arch.  */
  if (sh64_isa_mode != sh64_isa_unspecified)
    val = EF_SH5;
  else
#endif /* HAVE_SH64 */
  if (valid_arch & arch_sh1)
    val = EF_SH1;
  else if (valid_arch & arch_sh2)
    val = EF_SH2;
  else if (valid_arch & arch_sh2e)
    val = EF_SH2E;
  else if (valid_arch & arch_sh_dsp)
    val = EF_SH_DSP;
  else if (valid_arch & arch_sh3)
    val = EF_SH3;
  else if (valid_arch & arch_sh3_dsp)
    val = EF_SH3_DSP;
  else if (valid_arch & arch_sh3e)
    val = EF_SH3E;
  else if (valid_arch & arch_sh4_nofpu)
    val = EF_SH4_NOFPU;
  else if (valid_arch & arch_sh4)
    val = EF_SH4;
  else if (valid_arch & arch_sh4a_nofpu)
    val = EF_SH4A_NOFPU;
  else if (valid_arch & arch_sh4a)
    val = EF_SH4A;
  else if (valid_arch & arch_sh4al_dsp)
    val = EF_SH4AL_DSP;
  else
    abort ();

  elf_elfheader (stdoutput)->e_flags &= ~EF_SH_MACH_MASK;
  elf_elfheader (stdoutput)->e_flags |= val;
}
#endif

/* Apply a fixup to the object file.  */

void
md_apply_fix3 (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  int lowbyte = target_big_endian ? 1 : 0;
  int highbyte = target_big_endian ? 0 : 1;
  long val = (long) *valP;
  long max, min;
  int shift;

#ifdef BFD_ASSEMBLER
  /* A difference between two symbols, the second of which is in the
     current section, is transformed in a PC-relative relocation to
     the other symbol.  We have to adjust the relocation type here.  */
  if (fixP->fx_pcrel)
    {
      switch (fixP->fx_r_type)
	{
	default:
	  break;

	case BFD_RELOC_32:
	  fixP->fx_r_type = BFD_RELOC_32_PCREL;
	  break;

	  /* Currently, we only support 32-bit PCREL relocations.
	     We'd need a new reloc type to handle 16_PCREL, and
	     8_PCREL is already taken for R_SH_SWITCH8, which
	     apparently does something completely different than what
	     we need.  FIXME.  */
	case BFD_RELOC_16:
	  bfd_set_error (bfd_error_bad_value);
	  return;

	case BFD_RELOC_8:
	  bfd_set_error (bfd_error_bad_value);
	  return;
	}
    }

  /* The function adjust_reloc_syms won't convert a reloc against a weak
     symbol into a reloc against a section, but bfd_install_relocation
     will screw up if the symbol is defined, so we have to adjust val here
     to avoid the screw up later.

     For ordinary relocs, this does not happen for ELF, since for ELF,
     bfd_install_relocation uses the "special function" field of the
     howto, and does not execute the code that needs to be undone, as long
     as the special function does not return bfd_reloc_continue.
     It can happen for GOT- and PLT-type relocs the way they are
     described in elf32-sh.c as they use bfd_elf_generic_reloc, but it
     doesn't matter here since those relocs don't use VAL; see below.  */
  if (OUTPUT_FLAVOR != bfd_target_elf_flavour
      && fixP->fx_addsy != NULL
      && S_IS_WEAK (fixP->fx_addsy))
    val -= S_GET_VALUE  (fixP->fx_addsy);
#endif

#ifdef BFD_ASSEMBLER
  if (SWITCH_TABLE (fixP))
    val -= S_GET_VALUE  (fixP->fx_subsy);
#else
  if (fixP->fx_r_type == 0)
    {
      if (fixP->fx_size == 2)
	fixP->fx_r_type = BFD_RELOC_16;
      else if (fixP->fx_size == 4)
	fixP->fx_r_type = BFD_RELOC_32;
      else if (fixP->fx_size == 1)
	fixP->fx_r_type = BFD_RELOC_8;
      else
	abort ();
    }
#endif

  max = min = 0;
  shift = 0;
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_SH_IMM4:
      max = 0xf;
      *buf = (*buf & 0xf0) | (val & 0xf);
      break;

    case BFD_RELOC_SH_IMM4BY2:
      max = 0xf;
      shift = 1;
      *buf = (*buf & 0xf0) | ((val >> 1) & 0xf);
      break;

    case BFD_RELOC_SH_IMM4BY4:
      max = 0xf;
      shift = 2;
      *buf = (*buf & 0xf0) | ((val >> 2) & 0xf);
      break;

    case BFD_RELOC_SH_IMM8BY2:
      max = 0xff;
      shift = 1;
      *buf = val >> 1;
      break;

    case BFD_RELOC_SH_IMM8BY4:
      max = 0xff;
      shift = 2;
      *buf = val >> 2;
      break;

    case BFD_RELOC_8:
    case BFD_RELOC_SH_IMM8:
      /* Sometimes the 8 bit value is sign extended (e.g., add) and
         sometimes it is not (e.g., and).  We permit any 8 bit value.
         Note that adding further restrictions may invalidate
         reasonable looking assembly code, such as ``and -0x1,r0''.  */
      max = 0xff;
      min = -0xff;
      *buf++ = val;
      break;

    case BFD_RELOC_SH_PCRELIMM8BY4:
      /* The lower two bits of the PC are cleared before the
         displacement is added in.  We can assume that the destination
         is on a 4 byte boundary.  If this instruction is also on a 4
         byte boundary, then we want
	   (target - here) / 4
	 and target - here is a multiple of 4.
	 Otherwise, we are on a 2 byte boundary, and we want
	   (target - (here - 2)) / 4
	 and target - here is not a multiple of 4.  Computing
	   (target - (here - 2)) / 4 == (target - here + 2) / 4
	 works for both cases, since in the first case the addition of
	 2 will be removed by the division.  target - here is in the
	 variable val.  */
      val = (val + 2) / 4;
      if (val & ~0xff)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("pcrel too far"));
      buf[lowbyte] = val;
      break;

    case BFD_RELOC_SH_PCRELIMM8BY2:
      val /= 2;
      if (val & ~0xff)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("pcrel too far"));
      buf[lowbyte] = val;
      break;

    case BFD_RELOC_SH_PCDISP8BY2:
      val /= 2;
      if (val < -0x80 || val > 0x7f)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("pcrel too far"));
      buf[lowbyte] = val;
      break;

    case BFD_RELOC_SH_PCDISP12BY2:
      val /= 2;
      if (val < -0x800 || val > 0x7ff)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("pcrel too far"));
      buf[lowbyte] = val & 0xff;
      buf[highbyte] |= (val >> 8) & 0xf;
      break;

    case BFD_RELOC_32:
    case BFD_RELOC_32_PCREL:
      md_number_to_chars (buf, val, 4);
      break;

    case BFD_RELOC_16:
      md_number_to_chars (buf, val, 2);
      break;

    case BFD_RELOC_SH_USES:
      /* Pass the value into sh_coff_reloc_mangle.  */
      fixP->fx_addnumber = val;
      break;

    case BFD_RELOC_SH_COUNT:
    case BFD_RELOC_SH_ALIGN:
    case BFD_RELOC_SH_CODE:
    case BFD_RELOC_SH_DATA:
    case BFD_RELOC_SH_LABEL:
      /* Nothing to do here.  */
      break;

    case BFD_RELOC_SH_LOOP_START:
    case BFD_RELOC_SH_LOOP_END:

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

#ifdef OBJ_ELF
    case BFD_RELOC_32_PLT_PCREL:
      /* Make the jump instruction point to the address of the operand.  At
	 runtime we merely add the offset to the actual PLT entry.  */
      * valP = 0xfffffffc;
      val = fixP->fx_offset;
      if (fixP->fx_subsy)
	val -= S_GET_VALUE (fixP->fx_subsy);
      fixP->fx_addnumber = val;
      md_number_to_chars (buf, val, 4);
      break;

    case BFD_RELOC_SH_GOTPC:
      /* This is tough to explain.  We end up with this one if we have
         operands that look like "_GLOBAL_OFFSET_TABLE_+[.-.L284]".
         The goal here is to obtain the absolute address of the GOT,
         and it is strongly preferable from a performance point of
         view to avoid using a runtime relocation for this.  There are
         cases where you have something like:

         .long	_GLOBAL_OFFSET_TABLE_+[.-.L66]

         and here no correction would be required.  Internally in the
         assembler we treat operands of this form as not being pcrel
         since the '.' is explicitly mentioned, and I wonder whether
         it would simplify matters to do it this way.  Who knows.  In
         earlier versions of the PIC patches, the pcrel_adjust field
         was used to store the correction, but since the expression is
         not pcrel, I felt it would be confusing to do it this way.  */
      * valP -= 1;
      md_number_to_chars (buf, val, 4);
      break;

    case BFD_RELOC_SH_TLS_GD_32:
    case BFD_RELOC_SH_TLS_LD_32:
    case BFD_RELOC_SH_TLS_IE_32:
      S_SET_THREAD_LOCAL (fixP->fx_addsy);
      /* Fallthrough */
    case BFD_RELOC_32_GOT_PCREL:
    case BFD_RELOC_SH_GOTPLT32:
      * valP = 0; /* Fully resolved at runtime.  No addend.  */
      md_number_to_chars (buf, 0, 4);
      break;

    case BFD_RELOC_SH_TLS_LDO_32:
    case BFD_RELOC_SH_TLS_LE_32:
      S_SET_THREAD_LOCAL (fixP->fx_addsy);
      /* Fallthrough */
    case BFD_RELOC_32_GOTOFF:
      md_number_to_chars (buf, val, 4);
      break;
#endif

    default:
#ifdef HAVE_SH64
      shmedia_md_apply_fix3 (fixP, valP);
      return;
#else
      abort ();
#endif
    }

  if (shift != 0)
    {
      if ((val & ((1 << shift) - 1)) != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("misaligned offset"));
      if (val >= 0)
	val >>= shift;
      else
	val = ((val >> shift)
	       | ((long) -1 & ~ ((long) -1 >> shift)));
    }
  if (max != 0 && (val < min || val > max))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("offset out of range"));

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Called just before address relaxation.  Return the length
   by which a fragment must grow to reach it's destination.  */

int
md_estimate_size_before_relax (fragS *fragP, segT segment_type)
{
  int what;

  switch (fragP->fr_subtype)
    {
    default:
#ifdef HAVE_SH64
      return shmedia_md_estimate_size_before_relax (fragP, segment_type);
#else
      abort ();
#endif


    case C (UNCOND_JUMP, UNDEF_DISP):
      /* Used to be a branch to somewhere which was unknown.  */
      if (!fragP->fr_symbol)
	{
	  fragP->fr_subtype = C (UNCOND_JUMP, UNCOND12);
	}
      else if (S_GET_SEGMENT (fragP->fr_symbol) == segment_type)
	{
	  fragP->fr_subtype = C (UNCOND_JUMP, UNCOND12);
	}
      else
	{
	  fragP->fr_subtype = C (UNCOND_JUMP, UNDEF_WORD_DISP);
	}
      break;

    case C (COND_JUMP, UNDEF_DISP):
    case C (COND_JUMP_DELAY, UNDEF_DISP):
      what = GET_WHAT (fragP->fr_subtype);
      /* Used to be a branch to somewhere which was unknown.  */
      if (fragP->fr_symbol
	  && S_GET_SEGMENT (fragP->fr_symbol) == segment_type)
	{
	  /* Got a symbol and it's defined in this segment, become byte
	     sized - maybe it will fix up.  */
	  fragP->fr_subtype = C (what, COND8);
	}
      else if (fragP->fr_symbol)
	{
	  /* Its got a segment, but its not ours, so it will always be long.  */
	  fragP->fr_subtype = C (what, UNDEF_WORD_DISP);
	}
      else
	{
	  /* We know the abs value.  */
	  fragP->fr_subtype = C (what, COND8);
	}
      break;

    case C (UNCOND_JUMP, UNCOND12):
    case C (UNCOND_JUMP, UNCOND32):
    case C (UNCOND_JUMP, UNDEF_WORD_DISP):
    case C (COND_JUMP, COND8):
    case C (COND_JUMP, COND12):
    case C (COND_JUMP, COND32):
    case C (COND_JUMP, UNDEF_WORD_DISP):
    case C (COND_JUMP_DELAY, COND8):
    case C (COND_JUMP_DELAY, COND12):
    case C (COND_JUMP_DELAY, COND32):
    case C (COND_JUMP_DELAY, UNDEF_WORD_DISP):
      /* When relaxing a section for the second time, we don't need to
	 do anything besides return the current size.  */
      break;
    }

  fragP->fr_var = md_relax_table[fragP->fr_subtype].rlx_length;
  return fragP->fr_var;
}

/* Put number into target byte order.  */

void
md_number_to_chars (char *ptr, valueT use, int nbytes)
{
#ifdef HAVE_SH64
  /* We might need to set the contents type to data.  */
  sh64_flag_output ();
#endif

  if (! target_big_endian)
    number_to_chars_littleendian (ptr, use, nbytes);
  else
    number_to_chars_bigendian (ptr, use, nbytes);
}

/* This version is used in obj-coff.c when not using BFD_ASSEMBLER.
   eg for the sh-hms target.  */

long
md_pcrel_from (fixS *fixP)
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address + 2;
}

long
md_pcrel_from_section (fixS *fixP, segT sec)
{
  if (! sh_local_pcrel (fixP)
      && fixP->fx_addsy != (symbolS *) NULL
      && (generic_force_reloc (fixP)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section,
	 or we're not sure about it being the final definition).  Let the
	 linker figure it out.  We need to adjust the subtraction of a
	 symbol to the position of the relocated data, though.  */
      return fixP->fx_subsy ? fixP->fx_where + fixP->fx_frag->fr_address : 0;
    }

  return md_pcrel_from (fixP);
}

#ifdef OBJ_COFF

int
tc_coff_sizemachdep (fragS *frag)
{
  return md_relax_table[frag->fr_subtype].rlx_length;
}

#endif /* OBJ_COFF */

#ifndef BFD_ASSEMBLER
#ifdef OBJ_COFF

/* Map BFD relocs to SH COFF relocs.  */

struct reloc_map
{
  bfd_reloc_code_real_type bfd_reloc;
  int sh_reloc;
};

static const struct reloc_map coff_reloc_map[] =
{
  { BFD_RELOC_32, R_SH_IMM32 },
  { BFD_RELOC_16, R_SH_IMM16 },
  { BFD_RELOC_8, R_SH_IMM8 },
  { BFD_RELOC_SH_PCDISP8BY2, R_SH_PCDISP8BY2 },
  { BFD_RELOC_SH_PCDISP12BY2, R_SH_PCDISP },
  { BFD_RELOC_SH_IMM4, R_SH_IMM4 },
  { BFD_RELOC_SH_IMM4BY2, R_SH_IMM4BY2 },
  { BFD_RELOC_SH_IMM4BY4, R_SH_IMM4BY4 },
  { BFD_RELOC_SH_IMM8, R_SH_IMM8 },
  { BFD_RELOC_SH_IMM8BY2, R_SH_IMM8BY2 },
  { BFD_RELOC_SH_IMM8BY4, R_SH_IMM8BY4 },
  { BFD_RELOC_SH_PCRELIMM8BY2, R_SH_PCRELIMM8BY2 },
  { BFD_RELOC_SH_PCRELIMM8BY4, R_SH_PCRELIMM8BY4 },
  { BFD_RELOC_8_PCREL, R_SH_SWITCH8 },
  { BFD_RELOC_SH_SWITCH16, R_SH_SWITCH16 },
  { BFD_RELOC_SH_SWITCH32, R_SH_SWITCH32 },
  { BFD_RELOC_SH_USES, R_SH_USES },
  { BFD_RELOC_SH_COUNT, R_SH_COUNT },
  { BFD_RELOC_SH_ALIGN, R_SH_ALIGN },
  { BFD_RELOC_SH_CODE, R_SH_CODE },
  { BFD_RELOC_SH_DATA, R_SH_DATA },
  { BFD_RELOC_SH_LABEL, R_SH_LABEL },
  { BFD_RELOC_UNUSED, 0 }
};

/* Adjust a reloc for the SH.  This is similar to the generic code,
   but does some minor tweaking.  */

void
sh_coff_reloc_mangle (segment_info_type *seg, fixS *fix,
		      struct internal_reloc *intr, unsigned int paddr)
{
  symbolS *symbol_ptr = fix->fx_addsy;
  symbolS *dot;

  intr->r_vaddr = paddr + fix->fx_frag->fr_address + fix->fx_where;

  if (! SWITCH_TABLE (fix))
    {
      const struct reloc_map *rm;

      for (rm = coff_reloc_map; rm->bfd_reloc != BFD_RELOC_UNUSED; rm++)
	if (rm->bfd_reloc == (bfd_reloc_code_real_type) fix->fx_r_type)
	  break;
      if (rm->bfd_reloc == BFD_RELOC_UNUSED)
	as_bad_where (fix->fx_file, fix->fx_line,
		      _("Can not represent %s relocation in this object file format"),
		      bfd_get_reloc_code_name (fix->fx_r_type));
      intr->r_type = rm->sh_reloc;
      intr->r_offset = 0;
    }
  else
    {
      know (sh_relax);

      if (fix->fx_r_type == BFD_RELOC_16)
	intr->r_type = R_SH_SWITCH16;
      else if (fix->fx_r_type == BFD_RELOC_8)
	intr->r_type = R_SH_SWITCH8;
      else if (fix->fx_r_type == BFD_RELOC_32)
	intr->r_type = R_SH_SWITCH32;
      else
	abort ();

      /* For a switch reloc, we set r_offset to the difference between
         the reloc address and the subtrahend.  When the linker is
         doing relaxing, it can use the determine the starting and
         ending points of the switch difference expression.  */
      intr->r_offset = intr->r_vaddr - S_GET_VALUE (fix->fx_subsy);
    }

  /* PC relative relocs are always against the current section.  */
  if (symbol_ptr == NULL)
    {
      switch (fix->fx_r_type)
	{
	case BFD_RELOC_SH_PCRELIMM8BY2:
	case BFD_RELOC_SH_PCRELIMM8BY4:
	case BFD_RELOC_SH_PCDISP8BY2:
	case BFD_RELOC_SH_PCDISP12BY2:
	case BFD_RELOC_SH_USES:
	  symbol_ptr = seg->dot;
	  break;
	default:
	  break;
	}
    }

  if (fix->fx_r_type == BFD_RELOC_SH_USES)
    {
      /* We can't store the offset in the object file, since this
	 reloc does not take up any space, so we store it in r_offset.
	 The fx_addnumber field was set in md_apply_fix3.  */
      intr->r_offset = fix->fx_addnumber;
    }
  else if (fix->fx_r_type == BFD_RELOC_SH_COUNT)
    {
      /* We can't store the count in the object file, since this reloc
         does not take up any space, so we store it in r_offset.  The
         fx_offset field was set when the fixup was created in
         sh_coff_frob_file.  */
      intr->r_offset = fix->fx_offset;
      /* This reloc is always absolute.  */
      symbol_ptr = NULL;
    }
  else if (fix->fx_r_type == BFD_RELOC_SH_ALIGN)
    {
      /* Store the alignment in the r_offset field.  */
      intr->r_offset = fix->fx_offset;
      /* This reloc is always absolute.  */
      symbol_ptr = NULL;
    }
  else if (fix->fx_r_type == BFD_RELOC_SH_CODE
	   || fix->fx_r_type == BFD_RELOC_SH_DATA
	   || fix->fx_r_type == BFD_RELOC_SH_LABEL)
    {
      /* These relocs are always absolute.  */
      symbol_ptr = NULL;
    }

  /* Turn the segment of the symbol into an offset.  */
  if (symbol_ptr != NULL)
    {
      dot = segment_info[S_GET_SEGMENT (symbol_ptr)].dot;
      if (dot != NULL)
	intr->r_symndx = dot->sy_number;
      else
	intr->r_symndx = symbol_ptr->sy_number;
    }
  else
    intr->r_symndx = -1;
}

#endif /* OBJ_COFF */
#endif /* ! BFD_ASSEMBLER */

#ifdef BFD_ASSEMBLER

/* Create a reloc.  */

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;

  r_type = fixp->fx_r_type;

  if (SWITCH_TABLE (fixp))
    {
      *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_subsy);
      rel->addend = 0;
      if (r_type == BFD_RELOC_16)
	r_type = BFD_RELOC_SH_SWITCH16;
      else if (r_type == BFD_RELOC_8)
	r_type = BFD_RELOC_8_PCREL;
      else if (r_type == BFD_RELOC_32)
	r_type = BFD_RELOC_SH_SWITCH32;
      else
	abort ();
    }
  else if (r_type == BFD_RELOC_SH_USES)
    rel->addend = fixp->fx_addnumber;
  else if (r_type == BFD_RELOC_SH_COUNT)
    rel->addend = fixp->fx_offset;
  else if (r_type == BFD_RELOC_SH_ALIGN)
    rel->addend = fixp->fx_offset;
  else if (r_type == BFD_RELOC_VTABLE_INHERIT
           || r_type == BFD_RELOC_VTABLE_ENTRY)
    rel->addend = fixp->fx_offset;
  else if (r_type == BFD_RELOC_SH_LOOP_START
           || r_type == BFD_RELOC_SH_LOOP_END)
    rel->addend = fixp->fx_offset;
  else if (r_type == BFD_RELOC_SH_LABEL && fixp->fx_pcrel)
    {
      rel->addend = 0;
      rel->address = rel->addend = fixp->fx_offset;
    }
#ifdef HAVE_SH64
  else if (shmedia_init_reloc (rel, fixp))
    ;
#endif
  else if (fixp->fx_pcrel)
    rel->addend = fixp->fx_addnumber;
  else if (r_type == BFD_RELOC_32 || r_type == BFD_RELOC_32_GOTOFF)
    rel->addend = fixp->fx_addnumber;
  else
    rel->addend = 0;

  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);

  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (r_type));
      /* Set howto to a garbage value so that we can keep going.  */
      rel->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_32);
      assert (rel->howto != NULL);
    }
#ifdef OBJ_ELF
  else if (rel->howto->type == R_SH_IND12W)
    rel->addend += fixp->fx_offset - 4;
#endif

  return rel;
}

#ifdef OBJ_ELF
inline static char *
sh_end_of_match (char *cont, char *what)
{
  int len = strlen (what);

  if (strncasecmp (cont, what, strlen (what)) == 0
      && ! is_part_of_name (cont[len]))
    return cont + len;

  return NULL;
}

int
sh_parse_name (char const *name, expressionS *exprP, char *nextcharP)
{
  char *next = input_line_pointer;
  char *next_end;
  int reloc_type;
  segT segment;

  exprP->X_op_symbol = NULL;

  if (strcmp (name, GLOBAL_OFFSET_TABLE_NAME) == 0)
    {
      if (! GOT_symbol)
	GOT_symbol = symbol_find_or_make (name);

      exprP->X_add_symbol = GOT_symbol;
    no_suffix:
      /* If we have an absolute symbol or a reg, then we know its
	     value now.  */
      segment = S_GET_SEGMENT (exprP->X_add_symbol);
      if (segment == absolute_section)
	{
	  exprP->X_op = O_constant;
	  exprP->X_add_number = S_GET_VALUE (exprP->X_add_symbol);
	  exprP->X_add_symbol = NULL;
	}
      else if (segment == reg_section)
	{
	  exprP->X_op = O_register;
	  exprP->X_add_number = S_GET_VALUE (exprP->X_add_symbol);
	  exprP->X_add_symbol = NULL;
	}
      else
	{
	  exprP->X_op = O_symbol;
	  exprP->X_add_number = 0;
	}

      return 1;
    }

  exprP->X_add_symbol = symbol_find_or_make (name);

  if (*nextcharP != '@')
    goto no_suffix;
  else if ((next_end = sh_end_of_match (next + 1, "GOTOFF")))
    reloc_type = BFD_RELOC_32_GOTOFF;
  else if ((next_end = sh_end_of_match (next + 1, "GOTPLT")))
    reloc_type = BFD_RELOC_SH_GOTPLT32;
  else if ((next_end = sh_end_of_match (next + 1, "GOT")))
    reloc_type = BFD_RELOC_32_GOT_PCREL;
  else if ((next_end = sh_end_of_match (next + 1, "PLT")))
    reloc_type = BFD_RELOC_32_PLT_PCREL;
  else if ((next_end = sh_end_of_match (next + 1, "TLSGD")))
    reloc_type = BFD_RELOC_SH_TLS_GD_32;
  else if ((next_end = sh_end_of_match (next + 1, "TLSLDM")))
    reloc_type = BFD_RELOC_SH_TLS_LD_32;
  else if ((next_end = sh_end_of_match (next + 1, "GOTTPOFF")))
    reloc_type = BFD_RELOC_SH_TLS_IE_32;
  else if ((next_end = sh_end_of_match (next + 1, "TPOFF")))
    reloc_type = BFD_RELOC_SH_TLS_LE_32;
  else if ((next_end = sh_end_of_match (next + 1, "DTPOFF")))
    reloc_type = BFD_RELOC_SH_TLS_LDO_32;
  else
    goto no_suffix;

  *input_line_pointer = *nextcharP;
  input_line_pointer = next_end;
  *nextcharP = *input_line_pointer;
  *input_line_pointer = '\0';

  exprP->X_op = O_PIC_reloc;
  exprP->X_add_number = 0;
  exprP->X_md = reloc_type;

  return 1;
}
#endif

void
sh_cfi_frame_initial_instructions (void)
{
  cfi_add_CFA_def_cfa (15, 0);
}

int
sh_regname_to_dw2regnum (const char *regname)
{
  unsigned int regnum = -1;
  unsigned int i;
  const char *p;
  char *q;
  static struct { char *name; int dw2regnum; } regnames[] =
    {
      { "pr", 17 }, { "t", 18 }, { "gbr", 19 }, { "mach", 20 },
      { "macl", 21 }, { "fpul", 23 }
    };

  for (i = 0; i < ARRAY_SIZE (regnames); ++i)
    if (strcmp (regnames[i].name, regname) == 0)
      return regnames[i].dw2regnum;

  if (regname[0] == 'r')
    {
      p = regname + 1;
      regnum = strtoul (p, &q, 10);
      if (p == q || *q || regnum >= 16)
	return -1;
    }
  else if (regname[0] == 'f' && regname[1] == 'r')
    {
      p = regname + 2;
      regnum = strtoul (p, &q, 10);
      if (p == q || *q || regnum >= 16)
	return -1;
      regnum += 25;
    }
  else if (regname[0] == 'x' && regname[1] == 'd')
    {
      p = regname + 2;
      regnum = strtoul (p, &q, 10);
      if (p == q || *q || regnum >= 8)
	return -1;
      regnum += 87;
    }
  return regnum;
}
#endif /* BFD_ASSEMBLER */
