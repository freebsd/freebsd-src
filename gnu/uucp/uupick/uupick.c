/* uupick.c
   Get files stored in the public directory by uucp -t.

   Copyright (C) 1992 Ian Lance Taylor

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
const char uupick_rcsid[] = "$Id: uupick.c,v 1.1 1993/08/05 18:27:50 conklin Exp $";
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

/* The program name.  */
char abProgram[] = "uupick";

/* Long getopt options.  */
static const struct option asPlongopts[] = { { NULL, 0, NULL, 0 } };

/* Local functions.  */

static void upusage P((void));

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
  struct uuconf_system ssys;
  const char *zpubdir;
  char *zfile, *zfrom, *zfull;
  char *zallsys;
  char ab[1000];
  boolean fquit;

  while ((iopt = getopt_long (argc, argv, "I:s:x:", asPlongopts,
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

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  upusage ();
	  break;
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
      iuuconf = uuconf_system_info (puuconf, zsystem, &ssys);
      if (iuuconf == UUCONF_SUCCESS)
	{
	  zpubdir = zbufcpy (ssys.uuconf_zpubdir);
	  (void) uuconf_system_free (puuconf, &ssys);
	}
      else if (iuuconf != UUCONF_NOT_FOUND)
	(void) ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
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
	      zlocal = zsysdep_uupick_local_file (zto);
	      if (zlocal == NULL)
		usysdep_exit (FALSE);
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

/* Print usage message.  */

static void
upusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uupick [-s system] [-I config] [-x debug]\n");
  fprintf (stderr,
	   " -s system: Only consider files from named system\n");
  fprintf (stderr,
	   " -x debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
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
