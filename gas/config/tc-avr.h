/* This file is tc-avr.h
   Copyright 1999, 2000, 2001, 2002, 2005 Free Software Foundation, Inc.

   Contributed by Denis Chertykov <denisc@overta.ru>

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* By convention, you should define this macro in the `.h' file.  For
   example, `tc-m68k.h' defines `TC_M68K'.  You might have to use this
   if it is necessary to add CPU specific code to the object format
   file.  */
#define TC_AVR

/* This macro is the BFD target name to use when creating the output
   file.  This will normally depend upon the `OBJ_FMT' macro.  */
#define TARGET_FORMAT "elf32-avr"

/* This macro is the BFD architecture to pass to `bfd_set_arch_mach'.  */
#define TARGET_ARCH bfd_arch_avr

/* This macro is the BFD machine number to pass to
   `bfd_set_arch_mach'.  If it is not defined, GAS will use 0.  */
#define TARGET_MACH 0

/* You should define this macro to be non-zero if the target is big
   endian, and zero if the target is little endian.  */
#define TARGET_BYTES_BIG_ENDIAN 0

/* If you define this macro, GAS will warn about the use of
   nonstandard escape sequences in a string.  */
#define ONLY_STANDARD_ESCAPES

/* GAS will call this function for any expression that can not be
   recognized.  When the function is called, `input_line_pointer'
   will point to the start of the expression.  */
#define md_operand(x)

/* You may define this macro to parse an expression used in a data
   allocation pseudo-op such as `.word'.  You can use this to
   recognize relocation directives that may appear in such directives.  */
#define TC_PARSE_CONS_EXPRESSION(EXPR,N) avr_parse_cons_expression (EXPR, N)
extern void avr_parse_cons_expression (expressionS *, int);

/* You may define this macro to generate a fixup for a data
   allocation pseudo-op.  */
#define TC_CONS_FIX_NEW(FRAG,WHERE,N,EXP) avr_cons_fix_new (FRAG, WHERE, N, EXP)
extern void avr_cons_fix_new (fragS *,int, int, expressionS *);

/* This should just call either `number_to_chars_bigendian' or
   `number_to_chars_littleendian', whichever is appropriate.  On
   targets like the MIPS which support options to change the
   endianness, which function to call is a runtime decision.  On
   other targets, `md_number_to_chars' can be a simple macro.  */
#define md_number_to_chars number_to_chars_littleendian

/* `md_short_jump_size'
   `md_long_jump_size'
   `md_create_short_jump'
   `md_create_long_jump'
   If `WORKING_DOT_WORD' is defined, GAS will not do broken word
   processing (*note Broken words::.).  Otherwise, you should set
   `md_short_jump_size' to the size of a short jump (a jump that is
   just long enough to jump around a long jmp) and
   `md_long_jump_size' to the size of a long jump (a jump that can go
   anywhere in the function), You should define
   `md_create_short_jump' to create a short jump around a long jump,
   and define `md_create_long_jump' to create a long jump.  */
#define WORKING_DOT_WORD

/* If you define this macro, it means that `tc_gen_reloc' may return
   multiple relocation entries for a single fixup.  In this case, the
   return value of `tc_gen_reloc' is a pointer to a null terminated
   array.  */
#undef RELOC_EXPANSION_POSSIBLE

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* If you define this macro, it should return the offset between the
   address of a PC relative fixup and the position from which the PC
   relative adjustment should be made.  On many processors, the base
   of a PC relative instruction is the next instruction, so this
   macro would return the length of an instruction.  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

/* The number of bytes to put into a word in a listing.  This affects
   the way the bytes are clumped together in the listing.  For
   example, a value of 2 might print `1234 5678' where a value of 1
   would print `12 34 56 78'.  The default value is 4.  */
#define LISTING_WORD_SIZE 2

/* AVR port uses `$' as a logical line separator */
#define LEX_DOLLAR 0

/* An `.lcomm' directive with no explicit alignment parameter will
   use this macro to set P2VAR to the alignment that a request for
   SIZE bytes will have.  The alignment is expressed as a power of
   two.  If no alignment should take place, the macro definition
   should do nothing.  Some targets define a `.bss' directive that is
   also affected by this macro.  The default definition will set
   P2VAR to the truncated power of two of sizes up to eight bytes.  */
#define TC_IMPLICIT_LCOMM_ALIGNMENT(SIZE, P2VAR) (P2VAR) = 0

/* We don't want gas to fixup the following program memory related relocations.
   We will need them in case that we want to do linker relaxation.
   We could in principle keep these fixups in gas when not relaxing.
   However, there is no serious performance penilty when making the linker
   make the fixup work.  */
#define TC_VALIDATE_FIX(FIXP,SEG,SKIP)                     \
 if (   (FIXP->fx_r_type == BFD_RELOC_AVR_7_PCREL          \
      || FIXP->fx_r_type == BFD_RELOC_AVR_13_PCREL         \
      || FIXP->fx_r_type == BFD_RELOC_AVR_LO8_LDI_PM       \
      || FIXP->fx_r_type == BFD_RELOC_AVR_HI8_LDI_PM       \
      || FIXP->fx_r_type == BFD_RELOC_AVR_HH8_LDI_PM       \
      || FIXP->fx_r_type == BFD_RELOC_AVR_16_PM)           \
     && (FIXP->fx_addsy))                                  \
   {                                                       \
     goto SKIP;                                            \
   }

