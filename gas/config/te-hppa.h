/* Machine specific defines for the PA machine
   Copyright 1987, 1991, 1992, 1993, 1995, 2000
   Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* HP PA-RISC and OSF/1 support was contributed by the Center for
	Software Science at the University of Utah.  */

/* Labels are not required to have a colon for a suffix.  */
#define LABELS_WITHOUT_COLONS 1

/* These define interfaces.  */
#include "obj-format.h"
