/* tc-tic30.h -- Header file for tc-tic30.c
   Copyright (C) 1998 Free Software Foundation.
   Contributed by Steven Haworth (steve@pm.cse.rmit.edu.au)

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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _TC_TIC30_H_
#define _TC_TIC30_H_

#define TC_TIC30 1

#ifdef OBJ_AOUT
#define TARGET_FORMAT "a.out-tic30"
#endif

#define TARGET_ARCH		bfd_arch_tic30
#define TARGET_BYTES_BIG_ENDIAN	1

#define WORKING_DOT_WORD

char *output_invalid PARAMS ((int c));

#define END_OF_INSN '\0'
#define MAX_OPERANDS 6
#define DIRECT_REFERENCE '@'
#define INDIRECT_REFERENCE '*'
#define PARALLEL_SEPARATOR '|'
#define INSN_SIZE 4

/* Define this to 1 if you want the debug output to be on stdout,
   otherwise stderr will be used.  If stderr is used, there will be a
   better synchronisation with the as_bad outputs, but you can't
   capture the output. */
#define USE_STDOUT 0

#define tc_unrecognized_line tic30_unrecognized_line

extern int tic30_unrecognized_line PARAMS ((int));

#endif
