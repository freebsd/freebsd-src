/* BUMP_ALLOC macro - increase table allocation by one element.
   Copyright (C) 1990, 1991, 1993 Free Software Foundation, Inc.
   Francois Pinard <pinard@iro.umontreal.ca>, 1990.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*-------------------------------------------------------------------------.
| Bump the allocation of the array pointed to by TABLE whenever required.  |
| The table already has already COUNT elements in it, this macro ensure it |
| has enough space to accommodate at least one more element.  Space is	   |
| allocated (2 ^ EXPONENT) elements at a time.  Each element of the array  |
| is of type TYPE.							   |
`-------------------------------------------------------------------------*/

/* Routines `xmalloc' and `xrealloc' are called to do the actual memory
   management.  This implies that the program will abort with an `Virtual
   Memory exhausted!' error if any problem arise.
   
   To work correctly, at least EXPONENT and TYPE should always be the
   same for all uses of this macro for any given TABLE.  A secure way to
   achieve this is to never use this macro directly, but use it to define
   other macros, which would then be TABLE-specific.
   
   The first time through, COUNT is usually zero.  Note that COUNT is not
   updated by this macro, but it should be update elsewhere, later.  This
   is convenient, because it allows TABLE[COUNT] to refer to the new
   element at the end.  Once its construction is completed, COUNT++ will
   record it in the table.  Calling this macro several times in a row
   without updating COUNT is a bad thing to do.  */

#define BUMP_ALLOC(Table, Count, Exponent, Type) \
  BUMP_ALLOC_WITH_SIZE ((Table), (Count), (Exponent), Type, sizeof (Type))

/* In cases `sizeof TYPE' would not always yield the correct value for
   the size of each element entry, this macro accepts a supplementary
   SIZE argument.  The EXPONENT, TYPE and SIZE parameters should still
   have the same value for all macro calls related to a specific TABLE.  */

#define BUMP_ALLOC_WITH_SIZE(Table, Count, Exponent, Type, Size) \
  if (((Count) & (~(~0 << (Exponent)))) == 0)				\
    if ((Count) == 0)							\
      (Table) = (Type *) xmalloc ((1 << (Exponent)) * (Size));		\
    else								\
      (Table) = (Type *)						\
	xrealloc ((Table), ((Count) + (1 << (Exponent))) * (Size));	\
  else
