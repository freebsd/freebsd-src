/* ho-hpux.h -- Header to compile the assembler under HP-UX
   Copyright (C) 1988, 1991, 1992 Free Software Foundation, Inc.

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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "ho-sysv.h"

/* This header file contains the #defines specific
   to HPUX changes sent me by cph@zurich.ai.mit.edu */
#ifndef hpux
#define hpux
#endif

#ifdef setbuffer
#undef setbuffer
#endif /* setbuffer */

#define setbuffer(stream, buf, size)

/* end of ho-hpux.h */
