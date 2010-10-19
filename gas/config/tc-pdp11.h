/* tc-pdp11.h -- Header file for tc-pdp11.c.
   Copyright 2001, 2005 Free Software Foundation, Inc.

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

#define TC_PDP11 1

#define TARGET_FORMAT "a.out-pdp11"
#define TARGET_ARCH bfd_arch_pdp11
#define TARGET_BYTES_BIG_ENDIAN 0

#define LEX_TILDE (LEX_BEGIN_NAME | LEX_NAME)

#define md_operand(x)

long md_chars_to_number (unsigned char *, int);

/* end of tc-pdp11.h */
