/* memmove -- copy memory regions of arbitary length
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */


/*

NAME

	memmove -- copy memory regions of arbitary length

SYNOPSIS

	void memmove (void *out, const void *in, size_t n);

DESCRIPTION

	Copy LENGTH bytes from memory region pointed to by IN to memory
	region pointed to by OUT.

	Regions can be overlapping.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __STDC__
#include <stddef.h>
#else
#define size_t unsigned long
#endif

void *
memmove (out, in, length)
     void *out;
     const void* in;
     size_t length;
{
    bcopy(in, out, length);
    return out;
}
