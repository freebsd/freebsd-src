/* Append a string ontp another string
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* JF:  This is silly.  Why not stuff this in some other file? */
#ifdef USG
#define bcopy(from,to,n) memcpy(to,from,n)
#endif

void
append (charPP, fromP, length)
char	**charPP;
char	*fromP;
unsigned long length;
{
	if (length) {		/* Don't trust bcopy() of 0 chars. */
		bcopy (fromP, * charPP,(int) length);
		*charPP += length;
	}
}

/* end: append.c */
