/* Basic C++ demangling support for GDB.
   Copyright 1991, 1992 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support.

This file is part of GDB.

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


/*  This file contains support code for C++ demangling that is common
    to a styles of demangling, and GDB specific. */

#include "defs.h"
#include "command.h"
#include "gdbcmd.h"
#include "demangle.h"
#include <string.h>

/* Select the default C++ demangling style to use.  The default is "auto",
   which allows gdb to attempt to pick an appropriate demangling style for
   the executable it has loaded.  It can be set to a specific style ("gnu",
   "lucid", "arm", etc.) in which case gdb will never attempt to do auto
   selection of the style unless you do an explicit "set demangle auto".
   To select one of these as the default, set DEFAULT_DEMANGLING_STYLE in
   the appropriate target configuration file. */

#ifndef DEFAULT_DEMANGLING_STYLE
# define DEFAULT_DEMANGLING_STYLE AUTO_DEMANGLING_STYLE_STRING
#endif

/* String name for the current demangling style.  Set by the "set demangling"
   command, printed as part of the output by the "show demangling" command. */

static char *current_demangling_style_string;

/* List of supported demangling styles.  Contains the name of the style as
   seen by the user, and the enum value that corresponds to that style. */
   
static const struct demangler
{
  char *demangling_style_name;
  enum demangling_styles demangling_style;
  char *demangling_style_doc;
} demanglers [] =
{
  {AUTO_DEMANGLING_STYLE_STRING,
     auto_demangling,
     "Automatic selection based on executable"},
  {GNU_DEMANGLING_STYLE_STRING,
     gnu_demangling,
     "GNU (g++) style demangling"},
  {LUCID_DEMANGLING_STYLE_STRING,
     lucid_demangling,
     "Lucid (lcc) style demangling"},
  {ARM_DEMANGLING_STYLE_STRING,
     arm_demangling,
     "ARM style demangling"},
  {NULL, unknown_demangling, NULL}
};

/* show current demangling style. */

static void
show_demangling_command (ignore, from_tty)
   char *ignore;
   int from_tty;
{
  /* done automatically by show command. */
}


/* set current demangling style.  called by the "set demangling" command
   after it has updated the current_demangling_style_string to match
   what the user has entered.

   if the user has entered a string that matches a known demangling style
   name in the demanglers[] array then just leave the string alone and update
   the current_demangling_style enum value to match.

   if the user has entered a string that doesn't match, including an empty
   string, then print a list of the currently known styles and restore
   the current_demangling_style_string to match the current_demangling_style
   enum value.

   Note:  Assumes that current_demangling_style_string always points to
   a malloc'd string, even if it is a null-string. */

static void
set_demangling_command (ignore, from_tty)
   char *ignore;
   int from_tty;
{
  const struct demangler *dem;

  /*  First just try to match whatever style name the user supplied with
      one of the known ones.  Don't bother special casing for an empty
      name, we just treat it as any other style name that doesn't match.
      If we match, update the current demangling style enum. */

  for (dem = demanglers; dem -> demangling_style_name != NULL; dem++)
    {
      if (STREQ (current_demangling_style_string,
		  dem -> demangling_style_name))
	{
	  current_demangling_style = dem -> demangling_style;
	  break;
	}
    }

  /* Check to see if we found a match.  If not, gripe about any non-empty
     style name and supply a list of valid ones.  FIXME:  This should
     probably be done with some sort of completion and with help. */

  if (dem -> demangling_style_name == NULL)
    {
      if (*current_demangling_style_string != '\0')
	{
	  printf ("Unknown demangling style `%s'.\n",
		  current_demangling_style_string);
	}
      printf ("The currently understood settings are:\n\n");
      for (dem = demanglers; dem -> demangling_style_name != NULL; dem++)
	{
	  printf ("%-10s %s\n", dem -> demangling_style_name,
		  dem -> demangling_style_doc);
	  if (dem -> demangling_style == current_demangling_style)
	    {
	      free (current_demangling_style_string);
	      current_demangling_style_string =
		strdup (dem -> demangling_style_name);
	    }
	}
      if (current_demangling_style == unknown_demangling)
	{
	  /* This can happen during initialization if gdb is compiled with
	     a DEMANGLING_STYLE value that is unknown, so pick the first
	     one as the default. */
	  current_demangling_style = demanglers[0].demangling_style;
	  current_demangling_style_string =
	    strdup (demanglers[0].demangling_style_name);
	  warning ("`%s' style demangling chosen as the default.\n",
		   current_demangling_style_string);
	}
    }
}

/* Fake a "set demangling" command. */

void
set_demangling_style (style)
     char *style;
{
  if (current_demangling_style_string != NULL)
    {
      free (current_demangling_style_string);
    }
  current_demangling_style_string = strdup (style);
  set_demangling_command ((char *) NULL, 0);
}

void
_initialize_demangler ()
{
   struct cmd_list_element *set, *show;

   set = add_set_cmd ("demangle-style", class_support, var_string_noescape,
		      (char *) &current_demangling_style_string,
		      "Set the current C++ demangling style.\n\
Use `set demangle-style' without arguments for a list of demangling styles.",
		      &setlist);
   show = add_show_from_set (set, &showlist);
   set -> function.cfunc = set_demangling_command;
   show -> function.cfunc = show_demangling_command;

   /* Set the default demangling style chosen at compilation time. */
   set_demangling_style (DEFAULT_DEMANGLING_STYLE);
   set_cplus_marker_for_demangling (CPLUS_MARKER);
}
