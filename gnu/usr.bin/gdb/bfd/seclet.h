/* Definitions of little sections (seclets) for BFD.
   Copyright 1992 Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _SECLET_H
#define _SECLET_H

enum bfd_seclet_enum
{
  bfd_indirect_seclet,
  bfd_fill_seclet
};

struct bfd_seclet 
{
  struct bfd_seclet *next;
  enum bfd_seclet_enum type;
  unsigned int offset;  
  unsigned int size;
  union 
  {
    struct 
    {
      asection *section;
      asymbol **symbols;
    } indirect;
    struct {
      int value;
    } fill;
  }
  u;
};

typedef struct bfd_seclet bfd_seclet_type;

bfd_seclet_type *
bfd_new_seclet PARAMS ((bfd *, asection *));

#endif
