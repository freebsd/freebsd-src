/* Macro definitions for Sparc running under LynxOS.
   Copyright 1993, 1994 Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef TM_SPARCLYNX_H
#define TM_SPARCLYNX_H

#include "tm-lynx.h"

/* Use generic Sparc definitions. */
#include "sparc/tm-sparc.h"

/* Lynx does this backwards from everybody else */

#undef FRAME_SAVED_I0
#undef FRAME_SAVED_L0

#define FRAME_SAVED_I0 0
#define FRAME_SAVED_L0 (8 * REGISTER_RAW_SIZE (I0_REGNUM))

#endif /* TM_SPARCLYNX_H */
