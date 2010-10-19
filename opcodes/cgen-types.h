/* Type definitions for CGEN-based opcode libraries.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU opcodes library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef CGEN_TYPES_H
#define CGEN_TYPES_H

#include <stdint.h>

typedef  int8_t   QI;
typedef uint8_t  UQI;
typedef  int16_t  HI;
typedef uint16_t UHI;
typedef  int32_t  SI;
typedef uint32_t USI;
typedef  int64_t  DI;
typedef uint64_t UDI;

typedef int INT;
typedef unsigned int UINT;

typedef float SF;
typedef double DF;
typedef long double XF, TF;

#endif /* CGEN_TYPES_H */
