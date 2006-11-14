/* defer.c - handle "defered" links in newc and crc archives
   Copyright (C) 1993,2003,2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <system.h>

#include <stdio.h>
#include <sys/types.h>
#include "cpiohdr.h"
#include "extern.h"
#include "defer.h"

struct deferment *
create_deferment (struct new_cpio_header *file_hdr)
{
  struct deferment *d;
  d = (struct deferment *) xmalloc (sizeof (struct deferment) );
  d->header = *file_hdr;
  d->header.c_name = (char *) xmalloc (strlen (file_hdr->c_name) + 1);
  strcpy (d->header.c_name, file_hdr->c_name);
  return d;
}

void
free_deferment (struct deferment *d)
{
  free (d->header.c_name);
  free (d);
}
