/* ho-vax.h  Intended for vax vms
   Copyright (C) 1987, 1991, 1992 Free Software Foundation, Inc.
   
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

#define HO_VAX 1

#include "ho-vax.h"

/* We get better performance if we use the macros rather than the functions.*/
#include <ctype.h>

/* We need this to make sure that sys_nerr has the right Psect hack. */
#include <perror.h>

/* end of ho-vms.h */
