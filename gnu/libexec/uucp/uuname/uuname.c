/* uuname.c
   List the names of known remote UUCP sites.

   Copyright (C) 1991, 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uuname_rcsid[] = "$Id: uuname.c,v 1.1 1993/08/05 18:27:43 conklin Exp $";
#endif

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* The program name.  */
char abProgram[] = "uuname";

/* Local functions.  */

static void unusage P((void));
static void unuuconf_error P((pointer puuconf, int iuuconf));

/* Long getopt options.  */
static const struct option asNlongopts[] = { { NULL, 0, NULL, 0 } };

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -a: display aliases.  */
  boolean falias = FALSE;
  /* -l: if true, output local node name.  */
  boolean flocal = FALSE;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  int iopt;
  pointer puuconf;
  int iuuconf;

  while ((iopt = getopt_long (argc, argv, "alI:x:", asNlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'a':
	  /* Display aliases.  */
	  falias = TRUE;
	  break;

	case 'l':
	  /* Output local node name.  */
	  flocal = TRUE;
	  break;

	case 'I':
	  /* Configuration file name.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  unusage ();
	  break;
	}
    }

  if (optind != argc)
    unusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    unuuconf_error (puuconf, iuuconf);

#if DEBUG > 1
  {
    const char *zdebug;

    iuuconf = uuconf_debuglevel (puuconf, &zdebug);
    if (iuuconf != UUCONF_SUCCESS)
      ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
    if (zdebug != NULL)
      iDebug |= idebug_parse (zdebug);
  }
#endif

  usysdep_initialize (puuconf, INIT_SUID);

  if (flocal)
    {
      const char *zlocalname;

      iuuconf = uuconf_localname (puuconf, &zlocalname);
      if (iuuconf == UUCONF_NOT_FOUND)
	{
	  zlocalname = zsysdep_localname ();
	  if (zlocalname == NULL)
	    usysdep_exit (FALSE);
	}
      else if (iuuconf != UUCONF_SUCCESS)
	unuuconf_error (puuconf, iuuconf);
      printf ("%s\n", zlocalname);
    }
  else
    {
      char **pznames, **pz;

      iuuconf = uuconf_system_names (puuconf, &pznames, falias);
      if (iuuconf != UUCONF_SUCCESS)
	unuuconf_error (puuconf, iuuconf);

      for (pz = pznames; *pz != NULL; pz++)
	printf ("%s\n", *pz);
    }

  usysdep_exit (TRUE);

  /* Avoid warnings about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void
unusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uuname [-a] [-l] [-I file]\n");
  fprintf (stderr,
	   " -a: display aliases\n");
  fprintf (stderr,
	   " -l: print local name\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* Display a uuconf error and exit.  */

static void
unuuconf_error (puuconf, iret)
     pointer puuconf;
     int iret;
{
  char ab[512];

  (void) uuconf_error_string (puuconf, iret, ab, sizeof ab);
  if ((iret & UUCONF_ERROR_FILENAME) == 0)
    fprintf (stderr, "uuname: %s\n", ab);
  else
    fprintf (stderr, "uuname:%s\n", ab);
  exit (EXIT_FAILURE);
}
