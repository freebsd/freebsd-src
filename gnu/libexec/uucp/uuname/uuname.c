/* uuname.c
   List the names of known remote UUCP sites.

   Copyright (C) 1991, 1992, 1993, 1994, 1995 Ian Lance Taylor

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uuname_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uuname/uuname.c,v 1.6 1999/08/27 23:33:59 peter Exp $";
#endif

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* Local functions.  */

static void unusage P((void));
static void unhelp P((void));

/* Long getopt options.  */
static const struct option asNlongopts[] =
{
  { "aliases", no_argument, NULL, 'a' },
  { "local", no_argument, NULL, 'l' },
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

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

  zProgram = argv[0];

  while ((iopt = getopt_long (argc, argv, "alI:vx:", asNlongopts,
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

	case 'v':
	  /* Print version and exit.  */
	  printf ("%s: Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
		  zProgram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 1:
	  /* --help.  */
	  unhelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  unusage ();
	  /*NOTREACHED*/
	}
    }

  if (optind != argc)
    unusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

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

  usysdep_initialize (puuconf, INIT_SUID | INIT_NOCHDIR);

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
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
      printf ("%s\n", zlocalname);
    }
  else
    {
      char **pznames, **pz;

      iuuconf = uuconf_system_names (puuconf, &pznames, falias);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

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
	   "Usage: %s [-a] [-l] [-I file]\n", zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Print a help message.  */

static void unhelp ()
{
  printf ("Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
	   VERSION);
  printf ("Usage: %s [-a] [-l] [-I file]\n", zProgram);
  printf (" -a,--aliases: display aliases\n");
  printf (" -l,--local: print local name\n");
#if HAVE_TAYLOR_CONFIG
  printf (" -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  printf (" -v,--version: Print version and exit\n");
  printf (" --help: Print help and exit\n");
}
