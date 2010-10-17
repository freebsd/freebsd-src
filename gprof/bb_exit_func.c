/*  bb_exit_func.c - dumps all the basic-block statistics linked into
    the bb_head chain to .d files.

   Copyright 2000, 2001 Free Software Foundation, Inc.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.

   This code was contributed by:

     David Mosberger-Tang <David.Mosberger@acm.org>  */

#include <stdio.h>
#include <strings.h>
#include "bfd.h"
#include "gmon_out.h"

/* structure emitted by -a */
struct bb
{
  long zero_word;
  const char *filename;
  long *counts;
  long ncounts;
  struct bb *next;
  const unsigned long *addresses;
};

struct bb *__bb_head = (struct bb *) 0;


void
__bb_exit_func (void)
{
  const int version = GMON_VERSION;
  struct gmon_hdr ghdr;
  struct bb *ptr;
  FILE *fp;
  /* GEN_GMON_CNT_FILE should be defined on systems with mcleanup()
     functions that do not write basic-block to gmon.out.  In such
     cases profiling with "-pg -a" would result in a gmon.out file
     without basic-block info (because the file written here would be
     overwritten.  Thus, a separate file is generated instead.  The
     two files can easily be combined by specifying them on gprof's
     command line (and possibly generating a gmon.sum file with "gprof
     -s"). */
#ifndef GEN_GMON_CNT_FILE
#   define OUT_NAME	"gmon.out"
#else
#   define OUT_NAME	"gmon.cnt"
#endif
  fp = fopen (OUT_NAME, "wb");
  if (!fp)
    {
      perror (OUT_NAME);
      return;
    }
  memcpy (&ghdr.cookie[0], GMON_MAGIC, 4);
  memcpy (&ghdr.version, &version, sizeof (version));
  fwrite (&ghdr, sizeof (ghdr), 1, fp);

  for (ptr = __bb_head; ptr != 0; ptr = ptr->next)
    {
      u_int ncounts = ptr->ncounts;
      u_char tag;
      u_int i;

      tag = GMON_TAG_BB_COUNT;
      fwrite (&tag, sizeof (tag), 1, fp);
      fwrite (&ncounts, sizeof (ncounts), 1, fp);

      for (i = 0; i < ncounts; ++i)
	{
	  fwrite (&ptr->addresses[i], sizeof (ptr->addresses[0]), 1, fp);
	  fwrite (&ptr->counts[i], sizeof (ptr->counts[0]), 1, fp);
	}
    }
  fclose (fp);
}
