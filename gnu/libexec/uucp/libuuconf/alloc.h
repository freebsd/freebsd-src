/* alloc.h
   Header file for uuconf memory allocation routines.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

/* This header file is private to the uuconf memory allocation
   routines, and should not be included by any other files.  */

/* We want to be able to keep track of allocated memory blocks, so
   that we can free them up later.  This will let us free up all the
   memory allocated to hold information for a system, for example.  We
   do this by allocating large chunks and doling them out.  Calling
   uuconf_malloc_block will return a pointer to a magic cookie which
   can then be passed to uuconf_malloc and uuconf_free.  Passing the
   pointer to uuconf_free_block will free all memory allocated for
   that block.  */

/* We allocate this much space in each block.  On most systems, this
   will make the actual structure 1024 bytes, which may be convenient
   for some types of memory allocators.  */
#define CALLOC_SIZE (1008)

/* This is the actual structure of a block.  */
struct sblock
{
  /* Next block in linked list.  */
  struct sblock *qnext;
  /* Index of next free spot.  */
  size_t ifree;
  /* Last value returned by uuconf_malloc for this block.  */
  pointer plast;
  /* List of additional memory blocks.  */
  struct sadded *qadded;
  /* Buffer of data.  We put it in a union with a double to make sure
     it is adequately aligned.  */
  union
    {
      char ab[CALLOC_SIZE];
      double l;
    } u;
};

/* There is a linked list of additional memory blocks inserted by
   uuconf_add_block.  */
struct sadded
{
  /* The next in the list.  */
  struct sadded *qnext;
  /* The added block.  */
  pointer padded;
};
