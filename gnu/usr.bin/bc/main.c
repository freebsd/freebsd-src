/* main.c: The main program for bc.  */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "bcdefs.h"
#include <signal.h>
#include "global.h"
#include "proto.h"

/* Variables for processing multiple files. */
char   first_file;
extern FILE *yyin;


/* The main program for bc. */
int
main (argc, argv)
     int argc;
     char *argv[];
{
  int  ch; 
  
  /* Initialize many variables. */
  compile_only = FALSE;
  use_math = FALSE;
  warn_not_std = FALSE;
  std_only = FALSE;
  if (isatty(0) && isatty(1)) 
    interactive = TRUE;
  else
    interactive = FALSE;

  /* Parse the command line */
  ch = getopt (argc, argv, "lcisvw");
  while (ch != EOF)
    {
      switch (ch)
	{
	case 'c':  /* compile only */
	  compile_only = TRUE;
	  break;
	case 'l':  /* math lib */
	  use_math = TRUE;
	  break;
	case 'i':  /* force interactive */
	  interactive = TRUE;
	  break;
	case 'w':  /* Non standard features give warnings. */
	  warn_not_std = TRUE;
	  break;
	case 's':  /* Non standard features give errors. */
	  std_only = TRUE;
	  break;
	case 'v':  /* Print the version. */
	  printf ("%s\n", BC_VERSION);
	  break;
	}
      ch = getopt (argc, argv, "lcisvw");
    }

  /* Initialize the machine.  */
  init_storage();
  init_load();

  /* Set up interrupts to print a message. */
  if (interactive)
    signal (SIGINT, use_quit);

  /* Initialize the front end. */
  init_tree();
  init_gen ();
  g_argv = argv;
  g_argc = argc;
  is_std_in = FALSE;
  first_file = TRUE;
  if (!open_new_file ())
    exit (1);

  /* Do the parse. */
  yyparse ();

  /* End the compile only output with a newline. */
  if (compile_only)
    printf ("\n");

  exit (0);
}


/* This is the function that opens all the files. 
   It returns TRUE if the file was opened, otherwise
   it returns FALSE. */

int
open_new_file ()
{
  FILE *new_file;

  /* Set the line number. */
  line_no = 1;

  /* Check to see if we are done. */
  if (is_std_in) return (FALSE);

  /* Open the other files. */
  if (use_math && first_file)
    {
#ifdef BC_MATH_FILE
      /* Make the first file be the math library. */
      new_file = fopen (BC_MATH_FILE, "r");
      use_math = FALSE;
      if (new_file != NULL)
	{
	  new_yy_file (new_file);
	  return TRUE;
	}	
      else
	{
	  fprintf (stderr, "Math Library unavailable.\n");
	  exit (1);
	}
#else
      /* Load the code from a precompiled version of the math libarary. */
      extern char libmath[];
      char tmp;
      /* These MUST be in the order of first mention of each function.
	 That is why "a" comes before "c" even though "a" is defined after
	 after "c".  "a" is used in "s"! */
      tmp = lookup ("e", FUNCT);
      tmp = lookup ("l", FUNCT);
      tmp = lookup ("s", FUNCT);
      tmp = lookup ("a", FUNCT);
      tmp = lookup ("c", FUNCT);
      tmp = lookup ("j", FUNCT);
      load_code (libmath);
#endif
    }
  
  /* One of the argv values. */
  while (optind < g_argc)
    {
      new_file = fopen (g_argv[optind], "r");
      if (new_file != NULL)
	{
	  new_yy_file (new_file);
	  optind++;
	  return TRUE;
	}
      fprintf (stderr, "File %s is unavailable.\n", g_argv[optind++]);
      exit (1);
    }
  
  /* If we fall through to here, we should return stdin. */
  new_yy_file (stdin);
  is_std_in = TRUE;
  return TRUE;
}


/* Set yyin to the new file. */

void
new_yy_file (file)
     FILE *file;
{
  if (!first_file) fclose (yyin);
  yyin = file;
  first_file = FALSE;
}


/* Message to use quit.  */

void
use_quit (sig)
     int sig;
{
  printf ("\n(interrupt) use quit to exit.\n");
  signal (SIGINT, use_quit);
}
