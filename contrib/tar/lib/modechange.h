/* modechange.h -- definitions for file mode manipulation
   Copyright (C) 1989, 1990, 1997 Free Software Foundation, Inc.

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

/* Masks for the `flags' field in a `struct mode_change'. */

#if ! defined MODECHANGE_H_
# define MODECHANGE_H_

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# include <sys/types.h>

/* Affect the execute bits only if at least one execute bit is set already,
   or if the file is a directory. */
# define MODE_X_IF_ANY_X 01

/* If set, copy some existing permissions for u, g, or o onto the other two.
   Which of u, g, or o is copied is determined by which bits are set in the
   `value' field. */
# define MODE_COPY_EXISTING 02

struct mode_change
{
  char op;			/* One of "=+-". */
  char flags;			/* Special operations. */
  mode_t affected;		/* Set for u/g/o/s/s/t, if to be affected. */
  mode_t value;			/* Bits to add/remove. */
  struct mode_change *next;	/* Link to next change in list. */
};

/* Masks for mode_compile argument. */
# define MODE_MASK_EQUALS 1
# define MODE_MASK_PLUS 2
# define MODE_MASK_MINUS 4
# define MODE_MASK_ALL (MODE_MASK_EQUALS | MODE_MASK_PLUS | MODE_MASK_MINUS)

/* Error return values for mode_compile. */
# define MODE_INVALID (struct mode_change *) 0
# define MODE_MEMORY_EXHAUSTED (struct mode_change *) 1
# define MODE_BAD_REFERENCE (struct mode_change *) 2

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

struct mode_change *mode_compile PARAMS ((const char *, unsigned));
struct mode_change *mode_create_from_ref PARAMS ((const char *));
mode_t mode_adjust PARAMS ((mode_t, const struct mode_change *));
void mode_free PARAMS ((struct mode_change *));

#endif
