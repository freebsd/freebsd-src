/*
 * gripes.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.  
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

#include <stdio.h>
#include "gripes.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern int fprintf ();
extern int fflush ();
extern int exit ();
#endif

extern char *prognam;

void
gripe_no_name (section)
     char *section;
{
  if (section)
    fprintf (stderr, "What manual page do you want from section %s?\n",
	     section);
  else
    fprintf (stderr, "What manual page do you want?\n");

  fflush (stderr);
}

void
gripe_reading_man_file (name)
     char *name;
{
  fprintf (stderr, "Read access denied for file %s\n", name);

  fflush (stderr);
}

void
gripe_converting_name (name, to_cat)
     char *name;
     int to_cat;
{
  if (to_cat)
    fprintf (stderr, "Error converting %s to cat name\n", name);
  else
    fprintf (stderr, "Error converting %s to man name\n", name);

  fflush (stderr);

  exit (1);
}

void
gripe_system_command (status)
     int status;
{
  fprintf (stderr, "Error executing formatting or display command.\n");
  fprintf (stderr, "system command exited with status %d\n", status);

  fflush (stderr);
}

void
gripe_not_found (name, section)
     char *name, *section;
{
  if (section)
    fprintf (stderr, "No entry for %s in section %s of the manual\n",
	     name, section);
  else
    fprintf (stderr, "No manual entry for %s\n", name);

  fflush (stderr);
}

void
gripe_incompatible (s)
     char *s;
{
  fprintf (stderr, "%s: incompatible options %s\n", prognam, s);

  fflush (stderr);

  exit (1);
}

void
gripe_getting_mp_config (file)
     char *file;
{
  fprintf (stderr, "%s: unable to find the file %s\n", prognam, file);

  fflush (stderr);

  exit (1);
}

void
gripe_reading_mp_config (file)
     char *file;
{
  fprintf (stderr, "%s: unable to make sense of the file %s\n", prognam, file);

  fflush (stderr);

  exit (1);
}

void
gripe_invalid_section (section)
     char *section;
{
  fprintf (stderr, "%s: invalid section (%s) selected\n", prognam, section);

  fflush (stderr);

  exit (1);
}

void
gripe_manpath ()
{
  fprintf (stderr, "%s: manpath is null\n", prognam);

  fflush (stderr);

  exit (1);
}

void
gripe_alloc (bytes, object)
     int bytes;
     char *object;
{
  fprintf (stderr, "%s: can't malloc %d bytes for %s\n",
	   prognam, bytes, object);

  fflush (stderr);

  exit (1);
}

void
gripe_roff_command_from_file (file)
     char *file;
{
  fprintf (stderr, "Error parsing *roff command from file %s\n", file);

  fflush (stderr);
}

void
gripe_roff_command_from_env ()
{
  fprintf (stderr, "Error parsing MANROFFSEQ.  Using system defaults.\n");

  fflush (stderr);
}

void
gripe_roff_command_from_command_line ()
{
  fprintf (stderr, "Error parsing *roff command from command line.\n");

  fflush (stderr);
}
