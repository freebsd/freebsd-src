/* obj-evax.c - EVAX (openVMS/Alpha) object file format.
   Copyright 1996, 1997 Free Software Foundation, Inc.
   Contributed by Klaus Kämpf (kkaempf@progis.de) of
     proGIS Software, Aachen, Germany.

   This file is part of GAS, the GNU Assembler

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#define OBJ_HEADER "obj-evax.h"

#include "as.h"

static void s_evax_weak PARAMS ((int));

const pseudo_typeS obj_pseudo_table[] =
{
  { "weak", s_evax_weak, 0},
  {0, 0, 0},
};				/* obj_pseudo_table */

void obj_read_begin_hook () {}

/* Handle the weak specific pseudo-op.  */

static void
s_evax_weak (ignore)
     int ignore;
{
  char *name;
  int c;
  symbolS *symbolP;
  char *stop = NULL;
  char stopc;

  if (flag_mri)
    stop = mri_comment_field (&stopc);

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;
      SKIP_WHITESPACE ();
      S_SET_WEAK (symbolP);
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  if (flag_mri)
    mri_comment_end (stop, stopc);

  demand_empty_rest_of_line ();
}

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-evax.c */
