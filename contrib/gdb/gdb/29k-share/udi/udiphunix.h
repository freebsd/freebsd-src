/* Originally called "udiphsun.h", however it was not very
   Sun-specific; now it is used for generic-unix-with-bsd-ipc.

   Copyright 1993 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This file is to be used to reconfigure the UDI Procedural interface
   for a given host. This file should be placed so that it will be
   included from udiproc.h. Everything in here may need to be changed
   when you change either the host CPU or its compiler. Nothing in
   here should change to support different targets. There are multiple
   versions of this file, one for each of the different host/compiler
   combinations in use.
*/

#define UDIStruct  struct		/* _packed not needed on unix */
/* First, we need some types */
/* Types with at least the specified number of bits */
typedef double		UDIReal64;		/* 64-bit real value */
typedef float		UDIReal32;		/* 32-bit real value */
  
typedef unsigned long	UDIUInt32;		/* unsigned integers */
typedef unsigned short	UDIUInt16; 
typedef unsigned char	UDIUInt8;
  
typedef long		UDIInt32;		/* 32-bit integer */ 
typedef short		UDIInt16;		/* 16-bit integer */ 
typedef char		UDIInt8;		/* unreliable signedness */

/* To aid in supporting environments where the DFE and TIP use
different compilers or hosts (like DOS 386 on one side, 286 on the
other, or different Unix machines connected by sockets), we define
two abstract types - UDIInt and UDISizeT.
UDIInt should be defined to be int except for host/compiler combinations
that are intended to talk to existing UDI components that have a different
sized int. Similarly for UDISizeT.
*/
typedef int		UDIInt;
typedef unsigned int	UDIUInt;

typedef unsigned int	UDISizeT;

/* Now two void types. The first is for function return types,
the other for pointers to no particular type. Since these types
are used solely for documentational clarity, if your host/compiler
doesn't support either one, replace them with int and char *
respectively.
*/
typedef void		UDIVoid;		/* void type */
typedef void *		UDIVoidPtr;		/* void pointer type */
typedef void *		UDIHostMemPtr;		/* Arbitrary memory pointer */

/* Now we want a type optimized for boolean values. Normally this
   would be int, but on some machines (Z80s, 8051s, etc) it might
   be better to map it onto a char
*/
typedef	int		UDIBool;

/* Now indicate whether your compiler support full ANSI style
   prototypes. If so, use #if 1. If not use #if 0.
*/
#if 0
#define UDIParams(x)	x
#else
#define UDIParams(x)	()
#endif
