/* init.c
   Initialize the system dependent routines.

   Copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "sysdep.h"

#include <errno.h>
#include <pwd.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#if ! HAVE_GETHOSTNAME && HAVE_UNAME
#include <sys/utsname.h>
#endif

/* Use getcwd in preference to getwd; if we have neither, we will be
   using a getcwd replacement.  */
#if HAVE_GETCWD
#undef HAVE_GETWD
#define HAVE_GETWD 0
#else /* ! HAVE_GETCWD */
#if ! HAVE_GETWD
#undef HAVE_GETCWD
#define HAVE_GETCWD 1
#endif /* ! HAVE_GETWD */
#endif /* ! HAVE_GETCWD */

#if HAVE_GETWD
/* Get a value for MAXPATHLEN.  */
#if HAVE_SYS_PARAMS_H
#include <sys/params.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#else /* ! defined (PATH_MAX) */
#define MAXPATHLEN 1024
#endif /* ! defined (PATH_MAX) */
#endif /* ! defined (MAXPATHLEN) */
#endif /* HAVE_GETWD */

/* External functions.  */
#ifndef getlogin
extern char *getlogin ();
#endif
#if GETPWNAM_DECLARATION_OK
#ifndef getpwnam
extern struct passwd *getpwnam ();
#endif
#endif
#if GETPWUID_DECLARATION_OK
#ifndef getpwuid
extern struct passwd *getpwuid ();
#endif
#endif
#if HAVE_GETCWD
#ifndef getcwd
extern char *getcwd ();
#endif
#endif
#if HAVE_GETWD
#ifndef getwd
extern char *getwd ();
#endif
#endif
#if HAVE_SYSCONF
#ifndef sysconf
extern long sysconf ();
#endif
#endif

/* Initialize the system dependent routines.  We will probably be running
   suid to uucp, so we make sure that nothing is obviously wrong.  We
   save the login name since we will be losing the real uid.  */
static char *zSlogin;

/* The UUCP spool directory.  */
const char *zSspooldir;

/* The UUCP lock directory.  */
const char *zSlockdir;

/* The local UUCP name.  */
const char *zSlocalname;

/* We save the current directory since we will do a chdir to the
   spool directory.  */
char *zScwd;

/* The maximum length of a system name is controlled by the type of spool
   directory we use.  */
#if SPOOLDIR_V2 || SPOOLDIR_BSD42 || SPOOLDIR_BSD43 || SPOOLDIR_ULTRIX
size_t cSysdep_max_name_len = 7;
#endif
#if SPOOLDIR_HDB || SPOOLDIR_SVR4
size_t cSysdep_max_name_len = 14;
#endif
#if SPOOLDIR_TAYLOR
#if HAVE_LONG_FILE_NAMES
size_t cSysdep_max_name_len = 255;
#else /* ! HAVE_LONG_FILE_NAMES */
size_t cSysdep_max_name_len = 14;
#endif /* ! HAVE_LONG_FILE_NAMES */
#endif /* SPOOLDIR_TAYLOR */

/* Initialize the system dependent routines.  */

void
usysdep_initialize (puuconf,iflags)
     pointer puuconf;
     int iflags;
{
  int iuuconf;
  char *z;
  struct passwd *q;

  ulog_id (getpid ());

  if ((iflags & INIT_NOCLOSE) == 0)
    {
      int cdescs;
      int o;

      /* Close everything but stdin, stdout and stderr.  */
#if HAVE_GETDTABLESIZE
      cdescs = getdtablesize ();
#else
#if HAVE_SYSCONF
      cdescs = sysconf (_SC_OPEN_MAX);
#else
#ifdef OPEN_MAX
      cdescs = OPEN_MAX;
#else
#ifdef NOFILE
      cdescs = NOFILE;
#else
      cdescs = 20;
#endif /* ! defined (NOFILE) */
#endif /* ! defined (OPEN_MAX) */
#endif /* ! HAVE_SYSCONF */
#endif /* ! HAVE_GETDTABLESIZE */

      for (o = 3; o < cdescs; o++)
	(void) close (o);
    }

  /* Make sure stdin, stdout and stderr are open.  */
  if (fcntl (0, F_GETFD, 0) < 0
      && open ((char *) "/dev/null", O_RDONLY, 0) != 0)
    exit (EXIT_FAILURE);
  if (fcntl (1, F_GETFD, 0) < 0
      && open ((char *) "/dev/null", O_WRONLY, 0) != 1)
    exit (EXIT_FAILURE);
  if (fcntl (2, F_GETFD, 0) < 0
      && open ((char *) "/dev/null", O_WRONLY, 0) != 2)
    exit (EXIT_FAILURE);

  iuuconf = uuconf_spooldir (puuconf, &zSspooldir);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  iuuconf = uuconf_lockdir (puuconf, &zSlockdir);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  iuuconf = uuconf_localname (puuconf, &zSlocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
#if HAVE_GETHOSTNAME
      char ab[256];

      if (gethostname (ab, sizeof ab - 1) < 0)
	ulog (LOG_FATAL, "gethostname: %s", strerror (errno));
      ab[sizeof ab - 1] = '\0';
      ab[strcspn (ab, ".")] = '\0';
      zSlocalname = zbufcpy (ab);
#else /* ! HAVE_GETHOSTNAME */
#if HAVE_UNAME
      struct utsname s;

      if (uname (&s) < 0)
	ulog (LOG_FATAL, "uname: %s", strerror (errno));
      zSlocalname = zbufcpy (s.nodename);
#else /* ! HAVE_UNAME */
      ulog (LOG_FATAL, "Don't know how to get local node name");
#endif /* ! HAVE_UNAME */
#endif /* ! HAVE_GETHOSTNAME */
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  /* We always set our file modes to exactly what we want.  */
  umask (0);

  /* Get the login name, making sure that it matches the uid.  Many
     systems truncate the getlogin return value to 8 characters, but
     keep the full name in the password file, so we prefer the name in
     the password file.  */
  z = getenv ("LOGNAME");
  if (z == NULL)
    z = getenv ("USER");
  if (z == NULL)
    z = getlogin ();
  if (z == NULL)
    q = NULL;
  else
    {
      q = getpwnam (z);
      if (q != NULL)
	z = q->pw_name;
    }
  if (q == NULL || q->pw_uid != getuid ())
    {
      q = getpwuid (getuid ());
      if (q == NULL)
	z = NULL;
      else
	z = q->pw_name;
    }
  if (z != NULL)
    zSlogin = zbufcpy (z);

  /* On some old systems, an suid program run by root is started with
     an euid of 0.  If this happens, we look up the uid we should have
     and set ourselves to it manually.  This means that on such a
     system root will not be able to uucp or uux files that are not
     readable by uucp.  */
  if ((iflags & INIT_SUID) != 0
      && geteuid () == 0)
    {
      q = getpwnam (OWNER);
      if (q != NULL)
	setuid (q->pw_uid);
    }

  if ((iflags & INIT_GETCWD) != 0)
    {
      const char *zenv;
      struct stat senv, sdot;

      /* Get the current working directory.  We have to get it now,
	 since we're about to do a chdir.  We use PWD if it's defined
	 and if it really names the working directory, since if it's
	 not the same as whatever getcwd returns it's probably more
	 appropriate.  */
      zenv = getenv ("PWD");
      if (zenv != NULL
	  && stat ((char *) zenv, &senv) == 0
	  && stat ((char *) ".", &sdot) == 0
	  && senv.st_ino == sdot.st_ino
	  && senv.st_dev == sdot.st_dev)
	zScwd = zbufcpy (zenv);
      else
	{

#if HAVE_GETCWD
	  {
	    size_t c;

	    c = 128;
	    while (TRUE)
	      {
		zScwd = (char *) xmalloc (c);
		if (getcwd (zScwd, c) != NULL)
		  break;
		xfree ((pointer) zScwd);
		zScwd = NULL;
		if (errno != ERANGE)
		  break;
		c <<= 1;
	      }
	  }
#endif /* HAVE_GETCWD */

#if HAVE_GETWD
	  zScwd = (char *) xmalloc (MAXPATHLEN);
	  if (getwd (zScwd) == NULL)
	    {
	      xfree ((pointer) zScwd);
	      zScwd = NULL;
	    }
#endif /* HAVE_GETWD */

	  if (zScwd != NULL)
	    zScwd = (char *) xrealloc ((pointer) zScwd,
				       strlen (zScwd) + 1);
	}
    }

  if ((iflags & INIT_NOCHDIR) == 0)
    {
      /* Connect to the spool directory, and create it if it doesn't
	 exist.  */
      if (chdir (zSspooldir) < 0)
	{
	  if (errno == ENOENT
	      && mkdir ((char *) zSspooldir, IDIRECTORY_MODE) < 0)
	    ulog (LOG_FATAL, "mkdir (%s): %s", zSspooldir,
		  strerror (errno));
	  if (chdir (zSspooldir) < 0)
	    ulog (LOG_FATAL, "chdir (%s): %s", zSspooldir,
		  strerror (errno));
	}
    }
}

/* Exit the program.  */

void
usysdep_exit (fsuccess)
     boolean fsuccess;
{
  exit (fsuccess ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* This is called when a non-standard configuration file is used, to
   make sure the program doesn't hand out privileged file access.
   This means that to test non-standard configuration files, you
   should be logged in as uucp.  This is called before
   usysdep_initialize.  It ensures that someone can't simply use an
   alternate configuration file to steal UUCP transfers from other
   systems.  This will still permit people to set up their own
   configuration file and pretend to be whatever system they choose.
   The only real security is to use a high level of protection on the
   modem ports.  */

/*ARGSUSED*/
boolean fsysdep_other_config (z)
     const char *z;
{
  (void) setuid (getuid ());
  (void) setgid (getgid ());
  return TRUE;
}

/* Get the node name to use if it was not specified in the configuration
   file.  */

const char *
zsysdep_localname ()
{
  return zSlocalname;
}

/* Get the login name.  We actually get the login name in
   usysdep_initialize, because after that we may switch away from the
   real uid.  */

const char *
zsysdep_login_name ()
{
  if (zSlogin == NULL)
    ulog (LOG_FATAL, "Can't get login name");
  return zSlogin;
}
