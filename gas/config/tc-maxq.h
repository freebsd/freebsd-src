/* tc-maxq.h -- Header file for the asssembler(MAXQ)

   Copyright 2004, 2005  Free Software Foundation, Inc.

   Contributed by HCL Technologies Pvt. Ltd.

   Written by Vineet Sharma(vineets@noida.hcltech.com) Inderpreet
   S.(inderpreetb@noida.hcltech.com)

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef  _TC_MAXQ_H_
#define  _TC_MAXQ_H_

#ifndef NO_RELOC
#define NO_RELOC 0
#endif

/* `md_short_jump_size' `md_long_jump_size' `md_create_short_jump'
   `md_create_long_jump' If `WORKING_DOT_WORD' is defined, GAS will not do
   broken word processing (*note Broken words::.).  Otherwise, you should set
   `md_short_jump_size' to the size of a short jump (a jump that is just long
   enough to jump around a long jmp) and `md_long_jump_size' to the size of a
   long jump (a jump that can go anywhere in the function), You should define
   `md_create_short_jump' to create a short jump around a long jump, and
   define `md_create_long_jump' to create a long jump.  */
#define WORKING_DOT_WORD
typedef enum _RELOC_ENUM
{
  MAXQ_WORDDATA = 5,		/* Word+n.  */
  MAXQ_LONGDATA = 2,		/* Long+n.  */
  MAXQ_INTERSEGMENT = 4,	/* Text to any other segment.  */
  MAXQ_SHORTJUMP = BFD_RELOC_16_PCREL_S2,	/* PC Relative.  */
  MAXQ_LONGJUMP = 6,		/* Absolute Jump.  */
  EXTERNAL_RELOC = 8,
  INTERSEGMENT_RELOC
}
RELOC_ENUM;

#ifndef MAX_STACK
#define MAX_STACK 0xf
#endif

#ifndef TC_MAXQ20
#define TC_MAXQ20 1
#endif

#ifndef MAX_OPERAND_SIZE
#define MAX_OPERAND_SIZE 255
#endif

#ifndef MAXQ_INSTRUCTION_SIZE
#define MAXQ_INSTRUCTION_SIZE 2	/* 16 - BITS */
#endif

#if MAXQ_INSTRUCTION_SIZE
#define MAXQ_OCTETS_PER_BYTE 	MAXQ_INSTRUCTION_SIZE
#else
#define MAXQ_OCTETS_PER_BYTE 	OCTETS_PER_BYTE
#endif

/* if this macro is defined gas will use this instead of comment_chars.  */
#define tc_comments_chars maxq20_comment_chars

#define tc_coff_symbol_emit_hook(a)     ;	/* not used */

#define md_section_align(SEGMENT, SIZE)     (SIZE)

/* Locally defined symbol shoudnot be adjusted to section symbol.  */
#define tc_fix_adjustable(FIX) 0

/* This specifies that the target has been defined as little endian -
   default.  */
#define TARGET_BYTES_BIG_ENDIAN 0

#define MAX_MEM_NAME_SIZE 12
#define MAX_REG_NAME_SIZE  7
#define MAX_MNEM_SIZE      8

#define END_OF_INSN '\0'

/* This macro is the BFD archetectureto pass to 'bfd_set_arch_mach'.  */
#define TARGET_ARCH		bfd_arch_maxq

/* This macro is the BFD machine number to pass to 'bfd_set_arch_mach'.
   If not defines GAS will use 0.  */
#define TARGET_MACH     	maxq20_mach ()
extern unsigned long maxq20_mach (void);

#ifndef LEX_AT
/* We define this macro to generate a fixup for a data allocation pseudo-op.  */
#define TC_CONS_FIX_NEW(FRAG,OFF,LEN,EXP) maxq20_cons_fix_new (FRAG,OFF,LEN,EXP)
extern void maxq20_cons_fix_new (fragS *, unsigned int, unsigned int, expressionS *);
#endif

/* Define md_number_to_chars as the appropriate standard big endian or This
   should just call either `number_to_chars_bigendian' or
   `number_to_chars_littleendian', whichever is appropriate.  On targets like 
   the MIPS which support options to change the endianness, which function to 
   call is a runtime decision.  On other targets, `md_number_to_chars' can be 
   a simple macro.  */
#define md_number_to_chars maxq_number_to_chars
extern void maxq_number_to_chars (char *, valueT, int);

/* If this macro is defined, it is a pointer to a NULL terminated list of
   chracters which may appear in an operand. GAS already assumes that all
   alphanumeric chracters, and '$', '.', and '_' may appear in an
   operand("symbol_char"in app.c). This macro may be defined to treat
   additional chracters as appearing in an operand. This affects the way in
   which GAS removes whitespaces before passing the string to md_assemble.  */
#define tc_symbol_chars_extra_symbol_chars

/* Define away the call to md_operand in the expression parsing code. This is 
   called whenever the expression parser can't parse the input and gives the
   assembler backend a chance to deal with it instead.  */
#define md_operand(x)

#define MAX_OPERANDS           2	/* Max operands per instruction.  */
#define MAX_IMMEDIATE_OPERANDS 1	/* Max immediate operands per instruction.  */
#define MAX_MEMORY_OPERANDS    1	/* Max memory operands per instruction.  */

/* Define the prefix we are using while trying to use an immediate value in
   an instruction. e.g move A[0], #03h.  */
#define IMMEDIATE_PREFIX '#'

#define ABSOLUTE_PREFIX '@'

/* This here defines the opcode of the nop operation on the MAXQ. We did
   declare it here when we tried to fill the align bites with nop's but GAS
   only expects nop's to be single byte instruction.  */
#define NOP_OPCODE (char)0xDA3A

#define SIZE_OF_PM sizeof(pmodule)	/* Size of the structure.  */

#endif /* TC_MAXQ_H */
