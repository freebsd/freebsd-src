/* uupick.c
   Get files stored in the public directory by uucp -t.

   Copyright (C) 1992, 1993, 1994, 1995 Ian Lance Taylor

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
const char uupick_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uupick/uupick.c,v 1.6 1999/08/27 23:34:01 peter Exp $";
#endif

#include <errno.h>

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* Local functions.  */

static void upmovedir P((const char *zfull, const char *zrelative,
			 pointer pinfo));
static void upmove P((const char *zfrom, const char *zto));

/* Long getopt options.  */
static const struct option asPlongopts[] =
{
  { "system", required_argument, NULL, 's' },
  { "config", required_argument, NULL, 'I' },
  { "debug", required_argument, NULL, 'x' },
  { "version", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 1 },
  { NULL, 0, NULL, 0 }
};

/* Local functions.  */

static void upusage P((void));
static void uphelp P((void));

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -s: system name.  */
  const char *zsystem = NULL;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  int iopt;
  pointer puuconf;
  int iuuconf;
  const char *zpubdir;
  char *zfile, *zfrom, *zfull;
  char *zallsys;
  char ab[1000];
  boolean fquit;

  zProgram = "uupick";

  while ((iopt = getopt_long (argc, argv, "I:s:vx:", asPlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 's':
	  /* System name to get files from.  */
	  zsystem = optarg;
	  break;

	case 'I':
	  /* Name configuration file.  */
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
	  fprintf
	    (stderr,
	     "%s: Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
	     zProgram, VERSION);
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 1:
	  /* --help.  */
	  uphelp ();
	  exit (EXIT_SUCCESS);
	  /*NOTREACHED*/

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  upusage ();
	  /*NOTREACHED*/
	}
    }

  if (argc != optind)
    upusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  usysdep_initialize (puuconf, INIT_GETCWD | INIT_NOCHDIR);

  zpubdir = NULL;
  if (zsystem != NULL)
    {
      struct uuconf_system ssys;

      /* Get the public directory for the system.  If we can't find
         the system information, just use the standard public
         directory, since uupick is not setuid.  */
      iuuconf = uuconf_system_info (puuconf, zsystem, &ssys);
      if (iuuconf == UUCONF_SUCCESS)
	{
	  zpubdir = zbufcpy (ssys.uuconf_zpubdir);
	  (void) uuconf_system_free (puuconf, &ssys);
	}
    }
  if (zpubdir == NULL)
    {
      iuuconf = uuconf_pubdir (puuconf, &zpubdir);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
    }

  if (! fsysdep_uupick_init (zsystem, zpubdir))
    usysdep_exit (FALSE);

  zallsys = NULL;
  fquit = FALSE;

  while (! fquit
	 && ((zfile = zsysdep_uupick (zsystem, zpubdir, &zfrom, &zfull))
	     != NULL))
    {
      boolean fdir;
      char *zto, *zlocal;
      FILE *e;
      boolean fcontinue;

      fdir = fsysdep_directory (zfull);

      do
	{
	  boolean fbadname;

	  fcontinue = FALSE;

	  if (zallsys == NULL
	      || strcmp (zallsys, zfrom) != 0)
	    {
	      if (zallsys != NULL)
		{
		  ubuffree (zallsys);
		  zallsys = NULL;
		}

	      printf ("from %s: %s %s ?\n", zfrom, fdir ? "dir" : "file",
		      zfile);

	      if (fgets (ab, sizeof ab, stdin) == NULL)
		break;
	    }

	  if (ab[0] == 'q')
	    {
	      fquit = TRUE;
	      break;
	    }

	  switch (ab[0])
	    {
	    case '\n':
	      break;

	    case 'd':
	      if (fdir)
		(void) fsysdep_rmdir (zfull);
	      else
		{
		  if (remove (zfull) != 0)
		    ulog (LOG_ERROR, "remove (%s): %s", zfull,
			  strerror(errno));
		}
	      break;

	    case 'm':
	    case 'a':
	      zto = ab + 1 + strspn (ab + 1, " \t");
	      zto[strcspn (zto, " \t\n")] = '\0';
	      zlocal = zsysdep_uupick_local_file (zto, &fbadname);
	      if (zlocal == NULL)
		{
		  if (! fbadname)
		    usysdep_exit (FALSE);
		  ulog (LOG_ERROR, "%s: bad local file name", zto);
		  fcontinue = TRUE;
		  break;
		}
	      zto = zsysdep_in_dir (zlocal, zfile);
	      ubuffree (zlocal);
	      if (zto == NULL)
		usysdep_exit (FALSE);
	      if (! fdir)
		upmove (zfull, zto);
	      else
		{
		  usysdep_walk_tree (zfull, upmovedir, (pointer) zto);
		  (void) fsysdep_rmdir (zfull);
		}
	      ubuffree (zto);

	      if (ab[0] == 'a')
		{
		  zallsys = zbufcpy (zfrom);
		  ab[0] = 'm';
		}

	      break;

	    case 'p':
	      if (fdir)
		ulog (LOG_ERROR, "Can't print directory");
	      else
		{
		  e = fopen (zfull, "r");
		  if (e == NULL)
		    ulog (LOG_ERROR, "fopen (%s): %s", zfull,
			  strerror (errno));
		  else
		    {
		      while (fgets (ab, sizeof ab, e) != NULL)
			(void) fputs (ab, stdout);
		      (void) fclose (e);
		    }
		}
	      fcontinue = TRUE;
	      break;

	    case '!':
	      (void) system (ab + 1);
	      fcontinue = TRUE;
	      break;

	    default:
	      printf ("uupick commands:\n");
	      printf ("q: quit\n");
	      printf ("<return>: skip file\n");
	      printf ("m [dir]: move file to directory\n");
	      printf ("a [dir]: move all files from this system to directory\n");
	      printf ("p: list file to stdout\n");
	      printf ("d: delete file\n");
	      printf ("! command: shell escape\n");
	      fcontinue = TRUE;
	      break;
	    }
	}
      while (fcontinue);

      ubuffree (zfull);
      ubuffree (zfrom);
      ubuffree (zfile);
    }

  (void) fsysdep_uupick_free (zsystem, zpubdir);

  usysdep_exit (TRUE);

  /* Avoid error about not returning.  */
  return 0;
}

/* Print usage message and die.  */

static void
upusage ()
{
  fprintf (stderr,
	   "Usage: %s [-s system] [-I config] [-x debug]\n", zProgram);
  fprintf (stderr, "Use %s --help for help\n", zProgram);
  exit (EXIT_FAILURE);
}

/* Print help message.  */

static void
uphelp ()
{
  fprintf (stderr,
	   "Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   " -s,--system system: Only consider files from named system\n");
  fprintf (stderr,
	   " -x,--debug debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I,--config file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  fprintf (stderr,
	   " -v,--version: Print version and exit\n");
  fprintf (stderr,
	   " --help: Print help and exit\n");
}

/* This routine is called by usysdep_walk_tree when moving the
   contents of an entire directory.  */

static void
upmovedir (zfull, zrelative, pinfo)
     const char *zfull;
     const char *zrelative;
     pointer pinfo;
{
  const char *ztodir = (const char *) pinfo;
  char *zto;

  zto = zsysdep_in_dir (ztodir, zrelative);
  if (zto == NULL)
    usysdep_exit (FALSE);
  upmove (zfull, zto);
  ubuffree (zto);
}

/* Move a file.  */

static void
upmove (zfrom, zto)
     const char *zfrom;
     const char *zto;
{
  (void) fsysdep_move_file (zfrom, zto, TRUE, TRUE, FALSE,
			    (const char *) NULL);
}
