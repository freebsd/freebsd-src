/* Stack allocation routines.  This is intended for machines without support
   for the `alloca' function.

Copyright (C) 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

struct tmp_stack
{
  void *end;
  void *alloc_point;
  struct tmp_stack *prev;
};

struct tmp_marker
{
  struct tmp_stack *which_chunk;
  void *alloc_point;
};

typedef struct tmp_marker tmp_marker;

#if __STDC__
void *__tmp_alloc (unsigned long);
void __tmp_mark (tmp_marker *);
void __tmp_free (tmp_marker *);
#else
void *__tmp_alloc ();
void __tmp_mark ();
void __tmp_free ();
#endif

#ifndef __TMP_ALIGN
#define __TMP_ALIGN 8
#endif

#define TMP_DECL(marker) tmp_marker marker
#define TMP_ALLOC(size) \
  __tmp_alloc (((unsigned long) (size) + __TMP_ALIGN - 1) & -__TMP_ALIGN)
#define TMP_MARK(marker) __tmp_mark (&marker)
#define TMP_FREE(marker) __tmp_free (&marker)
