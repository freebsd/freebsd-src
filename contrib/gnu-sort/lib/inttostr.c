/* inttostr.c -- convert integers to printable strings

   Copyright (C) 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert */

#include "inttostr.h"

/* Convert I to a printable string in BUF, which must be at least
   INT_BUFSIZE_BOUND (INTTYPE) bytes long.  Return the address of the
   printable string, which need not start at BUF.  */

char *
inttostr (inttype i, char *buf)
{
  char *p = buf + INT_STRLEN_BOUND (inttype);
  *p = 0;

  if (i < 0)
    {
      do
	*--p = '0' - i % 10;
      while ((i /= 10) != 0);

      *--p = '-';
    }
  else
    {
      do
	*--p = '0' + i % 10;
      while ((i /= 10) != 0);
    }

  return p;
}
