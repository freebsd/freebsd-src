/* tc-i370.h -- Header file for tc-i370.c.
   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 2001, 2002, 2005
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

#define TC_I370

struct fix;

/* Set the endianness we are using.  Default to big endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 1
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH (i370_arch ())
extern enum bfd_architecture i370_arch (void);

/* Whether or not the target is big endian.  */
extern int target_big_endian;

/* The target BFD format.  */
#define TARGET_FORMAT ("elf32-i370")

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* $ is used to refer to the current location.  */
/* #define DOLLAR_DOT */

/* foo-. gets turned into PC relative relocs.  */
#define DIFF_EXPR_OK

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* Call md_pcrel_from_section, not md_pcrel_from.  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define md_operand(x)

#define tc_comment_chars i370_comment_chars
extern const char *i370_comment_chars;
