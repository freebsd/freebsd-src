/* IRIX targets
   Copyright 2002, 2003 Free Software Foundation, Inc.

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

/* This file is te-irix.h and is intended to provide support for
   IRIX targets.  Created by Alexandre Oliva <aoliva@redhat.com>.  */

#define TE_IRIX 1

/* these define interfaces */
#ifdef OBJ_HEADER
#include OBJ_HEADER
#else
#include "obj-format.h"
#endif
