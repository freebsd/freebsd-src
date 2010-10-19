/* tc-tic54x.h -- Header file for tc-tic54x.c
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Timothy Wall (twall@alum.mit.edu)

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

#ifndef _TC_TIC54X_H_
#define _TC_TIC54X_H_

/* select the proper coff format (see obj-coff.h) */
#define TC_TIC54X

#define TARGET_BYTES_BIG_ENDIAN	0
#define OCTETS_PER_BYTE_POWER 1

#define TARGET_ARCH		bfd_arch_tic54x

#define WORKING_DOT_WORD        1

#define MAX_OPERANDS 4
#define PARALLEL_SEPARATOR '|'
#define LABELS_WITHOUT_COLONS 1
/* accept 0FFFFh, 1010b, etc.  */
#define NUMBERS_WITH_SUFFIX 1
/* $ is section program counter */
#define DOLLAR_DOT 1
/* accept parallel lines like
       add #1,a  ||  ld #1, b
       (may also be split across lines)
*/
#define DOUBLEBAR_PARALLEL 1
/* affects preprocessor */
#define KEEP_WHITE_AROUND_COLON 1

struct bit_info
{
  segT seg;
#define TYPE_SPACE 0
#define TYPE_BES   1
#define TYPE_FIELD 2
  int type;
  symbolS *sym;
  valueT value;
  char *where;
  int offset;
};

/* We sometimes need to keep track of bit offsets within words */
#define TC_FRAG_TYPE int
#define TC_FRAG_INIT(FRAGP) do {(FRAGP)->tc_frag_data = 0;}while (0)

/* tell GAS whether the given token is indeed a code label */
#define TC_START_LABEL_WITHOUT_COLON(c,ptr) tic54x_start_label(c,ptr)
extern int tic54x_start_label PARAMS((int, char *));

/* custom handling for relocations in cons expressions */
#define TC_CONS_FIX_NEW(FRAG,OFF,LEN,EXP) tic54x_cons_fix_new(FRAG,OFF,LEN,EXP)
extern void tic54x_cons_fix_new PARAMS((fragS *,int,int,expressionS *));

/* Define md_number_to_chars as the appropriate standard big endian or
   little endian function.  Mostly littleendian, but longwords and floats are
   stored MS word first.
*/

#define md_number_to_chars tic54x_number_to_chars
extern void tic54x_number_to_chars (char *, valueT, int);
#define tc_adjust_symtab() tic54x_adjust_symtab()
extern void tic54x_adjust_symtab (void);
#define tc_unrecognized_line(ch) tic54x_unrecognized_line(ch)
extern int tic54x_unrecognized_line (int ch);
#define md_parse_name(s,e,m,c) tic54x_parse_name(s,e)
extern int tic54x_parse_name (char *name, expressionS *e);
#define md_undefined_symbol(s) tic54x_undefined_symbol(s)
extern symbolS *tic54x_undefined_symbol (char *name);
#define md_macro_start() tic54x_macro_start()
extern void tic54x_macro_start (void);
#define md_macro_end() tic54x_macro_end()
extern void tic54x_macro_end (void);
#define md_macro_info(args) tic54x_macro_info(args)
struct macro_struct;
extern void tic54x_macro_info PARAMS((const struct macro_struct *));
#define tc_frob_label(sym) tic54x_define_label (sym)
extern void tic54x_define_label PARAMS((symbolS *));

#define md_start_line_hook() tic54x_start_line_hook()
extern void tic54x_start_line_hook (void);

#define md_estimate_size_before_relax(f,s) \
tic54x_estimate_size_before_relax(f,s)
extern int tic54x_estimate_size_before_relax(fragS *, segT);

#define md_relax_frag(seg, f,s) tic54x_relax_frag(f,s)
extern int tic54x_relax_frag(fragS *, long);

#define md_convert_frag(b,s,f)	tic54x_convert_frag(b,s,f)
extern void tic54x_convert_frag(bfd *, segT, fragS *);

/* Other things we don't support...  */

/* Define away the call to md_operand in the expression parsing code.
   This is called whenever the expression parser can't parse the input
   and gives the assembler backend a chance to deal with it instead.  */

#define md_operand(X)

/* spruce up the listing output */
#define LISTING_WORD_SIZE 2

extern void tic54x_global (int);

#endif
