/* tc-arc.h - Macros and type defines for the ARC.
   Copyright (C) 1994, 1995, 1997 Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#define TC_ARC 1

#define TARGET_BYTES_BIG_ENDIAN 0

#define LOCAL_LABELS_FB 1

#define TARGET_ARCH bfd_arch_arc

#define LITTLE_ENDIAN   1234
#define BIG_ENDIAN      4321

/* The endianness of the target format may change based on command
   line arguments.  */
extern const char *arc_target_format;
#define DEFAULT_TARGET_FORMAT "elf32-littlearc"
#define TARGET_FORMAT arc_target_format
#define DEFAULT_BYTE_ORDER LITTLE_ENDIAN

#define WORKING_DOT_WORD

#define LISTING_HEADER "ARC GAS "

#define TC_HANDLES_FX_DONE

#define MD_APPLY_FIX3

/* The ARC needs to parse reloc specifiers in .word.  */

extern void arc_parse_cons_expression ();
#define TC_PARSE_CONS_EXPRESSION(EXP, NBYTES) \
arc_parse_cons_expression (EXP, NBYTES)

extern void arc_cons_fix_new ();
#define TC_CONS_FIX_NEW(FRAG, WHERE, NBYTES, EXP) \
arc_cons_fix_new (FRAG, WHERE, NBYTES, EXP)

#if 0
/* Extra stuff that we need to keep track of for each symbol.  */
struct arc_tc_sy
{
  /* The real name, if the symbol was renamed.  */
  char *real_name;
};

#define TC_SYMFIELD_TYPE struct arc_tc_sy

/* Finish up the symbol.  */
extern int arc_frob_symbol PARAMS ((struct symbol *));
#define tc_frob_symbol(sym, punt) punt = arc_frob_symbol (sym)
#endif
