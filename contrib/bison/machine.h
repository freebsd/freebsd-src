/* Define machine-dependencies for bison,
   Copyright (C) 1984, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef	eta10
#define	MAXSHORT	2147483647
#define	MINSHORT	-2147483648
#else
#define	MAXSHORT	32767
#define	MINSHORT	-32768
#endif

#if defined (MSDOS) && !defined (__GO32__)
#define	BITS_PER_WORD	16
#define MAXTABLE	16383
#else
#define	BITS_PER_WORD	32
#define MAXTABLE	32767
#endif

#define	WORDSIZE(n)	(((n) + BITS_PER_WORD - 1) / BITS_PER_WORD)
#define	SETBIT(x, i)	((x)[(i)/BITS_PER_WORD] |= (1<<((i) % BITS_PER_WORD)))
#define RESETBIT(x, i)	((x)[(i)/BITS_PER_WORD] &= ~(1<<((i) % BITS_PER_WORD)))
#define BITISSET(x, i)	(((x)[(i)/BITS_PER_WORD] & (1<<((i) % BITS_PER_WORD))) != 0)
