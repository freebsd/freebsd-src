/* This file is twe-ic960.h

   Copyright (C) 1987-1992 Free Software Foundation, Inc.

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

/*
 * This file is te-ic960.h and is intended to define ic960 environment
 * specific differences.
 */

#define TE_IC960 1

/* intel uses host byte order for headers */
#ifdef CROSS_COMPILE
#undef CROSS_COMPILE
#endif /* CROSS_COMPILE */

#define OBJ_COFF_OMIT_OPTIONAL_HEADER
#define LOCAL_LABEL(name) ( (name[0] == 'L') \
			   || (name[0] == '.' \
			       && (name[1] == 'C' || name[1] == 'I' || name[1] == '.')))
#include "obj-format.h"

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of te-ic960.h */
