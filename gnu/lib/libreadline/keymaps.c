/* keymaps.c -- Functions and keymaps for the GNU Readline library. */

/* Copyright (C) 1988,1989 Free Software Foundation, Inc.

   This file is part of GNU Readline, a library for reading lines
   of text with interactive input and history editing.

   Readline is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 1, or (at your option) any
   later version.

   Readline is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Readline; see the file COPYING.  If not, write to the Free
   Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "keymaps.h"
#include "emacs_keymap.c"

#ifdef VI_MODE
#include "vi_keymap.c"
#endif

/* Remove these declarations when we have a complete libgnu.a. */
#define STATIC_MALLOC
#ifndef STATIC_MALLOC
extern char *xmalloc (), *xrealloc ();
#else
static char *xmalloc (), *xrealloc ();
#endif

/* **************************************************************** */
/*								    */
/*		      Functions for manipulating Keymaps.	    */
/*								    */
/* **************************************************************** */


/* Return a new, empty keymap.
   Free it with free() when you are done. */
Keymap
rl_make_bare_keymap ()
{
  register int i;
  Keymap keymap = (Keymap)xmalloc (128 * sizeof (KEYMAP_ENTRY));

  for (i = 0; i < 128; i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = (Function *)NULL;
    }

  for (i = 'A'; i < ('Z' + 1); i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = rl_do_lowercase_version;
    }

  return (keymap);
}

/* Return a new keymap which is a copy of MAP. */
Keymap
rl_copy_keymap (map)
     Keymap map;
{
  register int i;
  Keymap temp = rl_make_bare_keymap ();

  for (i = 0; i < 128; i++)
    {
      temp[i].type = map[i].type;
      temp[i].function = map[i].function;
    }
  return (temp);
}

/* Return a new keymap with the printing characters bound to rl_insert,
   the uppercase Meta characters bound to run their lowercase equivalents,
   and the Meta digits bound to produce numeric arguments. */
Keymap
rl_make_keymap ()
{
  extern rl_insert (), rl_rubout (), rl_do_lowercase_version ();
  extern rl_digit_argument ();
  register int i;
  Keymap newmap;

  newmap = rl_make_bare_keymap ();

  /* All printing characters are self-inserting. */
  for (i = ' '; i < 126; i++)
    newmap[i].function = rl_insert;

  newmap[TAB].function = rl_insert;
  newmap[RUBOUT].function = rl_rubout;
  newmap[CTRL('H')].function = rl_rubout;

  return (newmap);
}

/* Free the storage associated with MAP. */
rl_discard_keymap (map)
     Keymap (map);
{
  int i;

  if (!map)
    return;

  for (i = 0; i < 128; i++)
    {
      switch (map[i].type)
	{
	case ISFUNC:
	  break;

	case ISKMAP:
	  rl_discard_keymap ((Keymap)map[i].function);
	  break;

	case ISMACR:
	  free ((char *)map[i].function);
	  break;
	}
    }
}

#ifdef STATIC_MALLOC

/* **************************************************************** */
/*								    */
/*			xmalloc and xrealloc ()		     	    */
/*								    */
/* **************************************************************** */

static void memory_error_and_abort ();

static char *
xmalloc (bytes)
     int bytes;
{
  char *temp = (char *)malloc (bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static char *
xrealloc (pointer, bytes)
     char *pointer;
     int bytes;
{
  char *temp = (char *)realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static void
memory_error_and_abort ()
{
  fprintf (stderr, "readline: Out of virtual memory!\n");
  abort ();
}
#endif /* STATIC_MALLOC */
