/* xqtsub.c
   System dependent functions used only by uuxqt.

   Copyright (C) 1991, 1992, 1993, 1995 Ian Lance Taylor

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
const char xqtsub_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "sysdep.h"

#include <ctype.h>
#include <errno.h>

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

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

/* Get a value for EX_TEMPFAIL.  */

#if HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

#ifndef EX_TEMPFAIL
#define EX_TEMPFAIL 75
#endif

/* Get the full pathname of the command to execute, given the list of
   permitted commands and the allowed path.  */

char *
zsysdep_find_command (zcmd, pzcmds, pzpath, pferr)
     const char *zcmd;
     char **pzcmds;
     char **pzpath;
     boolean *pferr;
{
  char **pz;
  struct stat s;

  *pferr = FALSE;

  for (pz = pzcmds; *pz != NULL; pz++)
    {
      char *zslash;

      if (strcmp (*pz, "ALL") == 0)
	break;

      zslash = strrchr (*pz, '/');
      if (zslash != NULL)
	++zslash;
      else
	zslash = *pz;
      if (strcmp (zslash, zcmd) == 0
	  || strcmp (*pz, zcmd) == 0)
	{
	  /* If we already have an absolute path, we can get out
	     immediately.  */
	  if (**pz == '/')
	    {
	      /* Quick error check.  */
	      if (stat (*pz, &s) != 0)
		{
		  ulog (LOG_ERROR, "%s: %s", *pz, strerror (errno));
		  *pferr = TRUE;
		  return NULL;
		}
	      return zbufcpy (*pz);
	    }
	  break;
	}
    }

  /* If we didn't find this command, get out.  */
  if (*pz == NULL)
    return NULL;

  /* We didn't find an absolute pathname, so we must look through
     the path.  */
  for (pz = pzpath; *pz != NULL; pz++)
    {
      char *zname;

      zname = zsysdep_in_dir (*pz, zcmd);
      if (stat (zname, &s) == 0)
	return zname;
    }

  return NULL;
}

/* Expand a local filename for uuxqt.  This is special because uuxqt
   only wants to expand filenames that start with ~ (it does not want
   to prepend the current directory to other names) and if the ~ is
   double, it is turned into a single ~.  This returns NULL to
   indicate that no change was required; it has no way to return
   error.  */

char *
zsysdep_xqt_local_file (qsys, zfile)
     const struct uuconf_system *qsys;
     const char *zfile;
{
  if (*zfile != '~')
    return NULL;
  if (zfile[1] == '~')
    {
      size_t clen;
      char *zret;

      clen = strlen (zfile);
      zret = zbufalc (clen);
      memcpy (zret, zfile + 1, clen);
      return zret;
    }
  return zsysdep_local_file (zfile, qsys->uuconf_zpubdir,
			     (boolean *) NULL);
}

#if ! ALLOW_FILENAME_ARGUMENTS

/* Check to see whether an argument specifies a file name; if it does,
   make sure that the file may legally be sent and/or received.  For
   Unix, we do not permit any occurrence of "/../" in the name, nor
   may it start with "../".  Otherwise, if it starts with "/" we check
   against the list of permitted files.  */

boolean
fsysdep_xqt_check_file (qsys, zfile)
     const struct uuconf_system *qsys;
     const char *zfile;
{
  size_t clen;

  /* Disallow exact "..", prefix "../", suffix "/..", internal "/../",
     and restricted absolute paths.  */
  clen = strlen (zfile);
  if ((clen == sizeof ".." - 1
       && strcmp (zfile, "..") == 0)
      || strncmp (zfile, "../", sizeof "../" - 1) == 0
      || (clen >= sizeof "/.." - 1
	  && strcmp (zfile + clen - (sizeof "/.." - 1), "/..") == 0)
      || strstr (zfile, "/../") != NULL
      || (*zfile == '/'
	  && (! fin_directory_list (zfile, qsys->uuconf_pzremote_send,
				    qsys->uuconf_zpubdir, TRUE, FALSE,
				    (const char *) NULL)
	      || ! fin_directory_list (zfile, qsys->uuconf_pzremote_receive,
				       qsys->uuconf_zpubdir, TRUE, FALSE,
				       (const char *) NULL))))
    {
      ulog (LOG_ERROR, "Not permitted to refer to file \"%s\"", zfile);
      return FALSE;
    }

  return TRUE;
}

#endif /* ! ALLOW_FILENAME_ARGUMENTS */

/* Invoke the command specified by an execute file.  */

/*ARGSUSED*/
boolean
fsysdep_execute (qsys, zuser, pazargs, zfullcmd, zinput, zoutput,
		 fshell, iseq, pzerror, pftemp)
     const struct uuconf_system *qsys;
     const char *zuser;
     const char **pazargs;
     const char *zfullcmd;
     const char *zinput;
     const char *zoutput;
     boolean fshell;
     int iseq;
     char **pzerror;
     boolean *pftemp;
{
  int aidescs[3];
  boolean ferr;
  pid_t ipid;
  int ierr;
  char abxqtdir[sizeof XQTDIR + 4];
  const char *zxqtdir;
  int istat;
  char *zpath;
#if ALLOW_SH_EXECUTION
  const char *azshargs[4];
#endif

  *pzerror = NULL;
  *pftemp = FALSE;

  aidescs[0] = SPAWN_NULL;
  aidescs[1] = SPAWN_NULL;
  aidescs[2] = SPAWN_NULL;

  ferr = FALSE;

  if (zinput != NULL)
    {
      aidescs[0] = open ((char *) zinput, O_RDONLY | O_NOCTTY, 0);
      if (aidescs[0] < 0)
	{
	  ulog (LOG_ERROR, "open (%s): %s", zinput, strerror (errno));
	  ferr = TRUE;
	}
      else if (fcntl (aidescs[0], F_SETFD,
		      fcntl (aidescs[0], F_GETFD, 0) | FD_CLOEXEC) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	  ferr = TRUE;
	}	
    }
  
  if (! ferr && zoutput != NULL)
    {
      aidescs[1] = creat ((char *) zoutput, IPRIVATE_FILE_MODE);
      if (aidescs[1] < 0)
	{
	  ulog (LOG_ERROR, "creat (%s): %s", zoutput, strerror (errno));
	  *pftemp = TRUE;
	  ferr = TRUE;
	}
      else if (fcntl (aidescs[1], F_SETFD,
		      fcntl (aidescs[1], F_GETFD, 0) | FD_CLOEXEC) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	  ferr = TRUE;
	}	
    }

  if (! ferr)
    {
      *pzerror = zstemp_file (qsys);
      aidescs[2] = creat (*pzerror, IPRIVATE_FILE_MODE);
      if (aidescs[2] < 0)
	{
	  if (errno == ENOENT)
	    {
	      if (! fsysdep_make_dirs (*pzerror, FALSE))
		{
		  *pftemp = TRUE;
		  ferr = TRUE;
		}
	      else
		aidescs[2] = creat (*pzerror, IPRIVATE_FILE_MODE);
	    }
	  if (! ferr && aidescs[2] < 0)
	    {
	      ulog (LOG_ERROR, "creat (%s): %s", *pzerror, strerror (errno));
	      *pftemp = TRUE;
	      ferr = TRUE;
	    }
	}
      if (! ferr
	  && fcntl (aidescs[2], F_SETFD,
		    fcntl (aidescs[2], F_GETFD, 0) | FD_CLOEXEC) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
	  ferr = TRUE;
	}	
    }

  if (iseq == 0)
    zxqtdir = XQTDIR;
  else
    {
      sprintf (abxqtdir, "%s%04d", XQTDIR, iseq);
      zxqtdir = abxqtdir;
    }

  if (ferr)
    {
      if (aidescs[0] != SPAWN_NULL)
	(void) close (aidescs[0]);
      if (aidescs[1] != SPAWN_NULL)
	(void) close (aidescs[1]);
      if (aidescs[2] != SPAWN_NULL)
	(void) close (aidescs[2]);
      ubuffree (*pzerror);
      return FALSE;
    }

#if ALLOW_SH_EXECUTION
  if (fshell)
    {
      azshargs[0] = "/bin/sh";
      azshargs[1] = "-c";
      azshargs[2] = zfullcmd;
      azshargs[3] = NULL;
      pazargs = azshargs;
    }
#else
  fshell = FALSE;
#endif

  if (qsys->uuconf_pzpath == NULL)
    zpath = NULL;
  else
    {
      size_t c;
      char **pz;

      c = 0;
      for (pz = qsys->uuconf_pzpath; *pz != NULL; pz++)
	c += strlen (*pz) + 1;
      zpath = zbufalc (c);
      *zpath = '\0';
      for (pz = qsys->uuconf_pzpath; *pz != NULL; pz++)
	{
	  strcat (zpath, *pz);
	  if (pz[1] != NULL)
	    strcat (zpath, ":");
	}
    }

  /* Pass zchdir as zxqtdir, fnosigs as TRUE, fshell as TRUE if we
     aren't already using the shell.  */
  ipid = ixsspawn (pazargs, aidescs, TRUE, FALSE, zxqtdir, TRUE,
		   ! fshell, zpath, qsys->uuconf_zname, zuser);

  ierr = errno;

  ubuffree (zpath);

  if (aidescs[0] != SPAWN_NULL)
    (void) close (aidescs[0]);
  if (aidescs[1] != SPAWN_NULL)
    (void) close (aidescs[1]);
  if (aidescs[2] != SPAWN_NULL)
    (void) close (aidescs[2]);

  if (ipid < 0)
    {
      ulog (LOG_ERROR, "ixsspawn: %s", strerror (ierr));
      *pftemp = TRUE;
      return FALSE;
    }

  istat = ixswait ((unsigned long) ipid, "Execution");

  if (istat == EX_TEMPFAIL)
    *pftemp = TRUE;

  return istat == 0;
}

/* Lock a uuxqt process.  */

int
ixsysdep_lock_uuxqt (zcmd, cmaxuuxqts)
     const char *zcmd;
     int cmaxuuxqts;
{
  char ab[sizeof "LCK.XQT.9999"];
  int i;

  if (cmaxuuxqts <= 0 || cmaxuuxqts >= 10000)
    cmaxuuxqts = 9999;
  for (i = 0; i < cmaxuuxqts; i++)
    {
      sprintf (ab, "LCK.XQT.%d", i);
      if (fsdo_lock (ab, TRUE, (boolean *) NULL))
	break;
    }
  if (i >= cmaxuuxqts)
    return -1;

  if (zcmd != NULL)
    {
      char abcmd[sizeof "LXQ.123456789"];

      sprintf (abcmd, "LXQ.%.9s", zcmd);
      abcmd[strcspn (abcmd, " \t/")] = '\0';
      if (! fsdo_lock (abcmd, TRUE, (boolean *) NULL))
	{
	  (void) fsdo_unlock (ab, TRUE);
	  return -1;
	}
    }

  return i;
}

/* Unlock a uuxqt process.  */

boolean
fsysdep_unlock_uuxqt (iseq, zcmd, cmaxuuxqts)
     int iseq;
     const char *zcmd;
     int cmaxuuxqts;
{
  char ab[sizeof "LCK.XQT.9999"];
  boolean fret;

  fret = TRUE;

  sprintf (ab, "LCK.XQT.%d", iseq);
  if (! fsdo_unlock (ab, TRUE))
    fret = FALSE;

  if (zcmd != NULL)
    {
      char abcmd[sizeof "LXQ.123456789"];

      sprintf (abcmd, "LXQ.%.9s", zcmd);
      abcmd[strcspn (abcmd, " \t/")] = '\0';
      if (! fsdo_unlock (abcmd, TRUE))
	fret = FALSE;
    }

  return fret;
}

/* See whether a particular uuxqt command is locked (this depends on
   the implementation of fsdo_lock).  */

boolean
fsysdep_uuxqt_locked (zcmd)
     const char *zcmd;
{
  char ab[sizeof "LXQ.123456789"];
  struct stat s;

  sprintf (ab, "LXQ.%.9s", zcmd);
  return stat (ab, &s) == 0;
}

/* Lock a particular execute file.  */

boolean
fsysdep_lock_uuxqt_file (zfile)
     const char *zfile;
{
  char *zcopy, *z;
  boolean fret;

  zcopy = zbufcpy (zfile);

  z = strrchr (zcopy, '/');
  if (z == NULL)
    *zcopy = 'L';
  else
    *(z + 1) = 'L';

  fret = fsdo_lock (zcopy, TRUE, (boolean *) NULL);
  ubuffree (zcopy);
  return fret;
}

/* Unlock a particular execute file.  */

boolean
fsysdep_unlock_uuxqt_file (zfile)
     const char *zfile;
{
  char *zcopy, *z;
  boolean fret;

  zcopy = zbufcpy (zfile);

  z = strrchr (zcopy, '/');
  if (z == NULL)
    *zcopy = 'L';
  else
    *(z + 1) = 'L';

  fret = fsdo_unlock (zcopy, TRUE);
  ubuffree (zcopy);
  return fret;
}

/* Lock the execute directory.  Since we use a different directory
   depending on which LCK.XQT.dddd file we got, there is actually no
   need to create a lock file.  We do make sure that the directory
   exists, though.  */

boolean
fsysdep_lock_uuxqt_dir (iseq)
     int iseq;
{
  const char *zxqtdir;
  char abxqtdir[sizeof XQTDIR + 4];

  if (iseq == 0)
    zxqtdir = XQTDIR;
  else
    {
      sprintf (abxqtdir, "%s%04d", XQTDIR, iseq);
      zxqtdir = abxqtdir;
    }

  if (mkdir (zxqtdir, S_IRWXU) < 0
      && errno != EEXIST
      && errno != EISDIR)
    {
      ulog (LOG_ERROR, "mkdir (%s): %s", zxqtdir, strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* Unlock the execute directory and clear it out.  The lock is
   actually the LCK.XQT.dddd file, so we don't unlock it, but we do
   remove all the files.  */

boolean
fsysdep_unlock_uuxqt_dir (iseq)
     int iseq;
{
  const char *zxqtdir;
  char abxqtdir[sizeof XQTDIR + 4];
  DIR *qdir;

  if (iseq == 0)
    zxqtdir = XQTDIR;
  else
    {
      sprintf (abxqtdir, "%s%04d", XQTDIR, iseq);
      zxqtdir = abxqtdir;
    }

  qdir = opendir ((char *) zxqtdir);
  if (qdir != NULL)
    {
      struct dirent *qentry;

      while ((qentry = readdir (qdir)) != NULL)
	{
	  char *z;

	  if (strcmp (qentry->d_name, ".") == 0
	      || strcmp (qentry->d_name, "..") == 0)
	    continue;
	  z = zsysdep_in_dir (zxqtdir, qentry->d_name);
	  if (remove (z) < 0)
	    {
	      int ierr;

	      ierr = errno;
	      if (! fsysdep_directory (z))
		ulog (LOG_ERROR, "remove (%s): %s", z,
		      strerror (ierr));
	      else
		(void) fsysdep_rmdir (z);
	    }
	  ubuffree (z);
	}

      closedir (qdir);
    }

  return TRUE;
}

/* Move files into the execution directory.  */

boolean
fsysdep_move_uuxqt_files (cfiles, pzfrom, pzto, fto, iseq, pzinput)
     int cfiles;
     const char *const *pzfrom;
     const char *const *pzto;
     boolean fto;
     int iseq;
     char **pzinput;
{
  char *zinput;
  const char *zxqtdir;
  char abxqtdir[sizeof XQTDIR + 4];
  int i;

  if (pzinput == NULL)
    zinput = NULL;
  else
    zinput = *pzinput;

  if (iseq == 0)
    zxqtdir = XQTDIR;
  else
    {
      sprintf (abxqtdir, "%s%04d", XQTDIR, iseq);
      zxqtdir = abxqtdir;
    }

  for (i = 0; i < cfiles; i++)
    {
      const char *zfrom, *zto;
      char *zfree;

      if (pzto[i] == NULL)
	continue;

      zfree = zsysdep_in_dir (zxqtdir, pzto[i]);

      zfrom = pzfrom[i];
      zto = zfree;

      if (zinput != NULL && strcmp (zinput, zfrom) == 0)
	{
	  *pzinput = zbufcpy (zto);
	  zinput = NULL;
	}

      if (! fto)
	{
	  const char *ztemp;
	  
	  ztemp = zfrom;
	  zfrom = zto;
	  zto = ztemp;
	  (void) chmod (zfrom, IPRIVATE_FILE_MODE);
	}

      if (rename (zfrom, zto) < 0)
	{
#if HAVE_RENAME
	  /* On some systems the system call rename seems to fail for
	     arbitrary reasons.  To get around this, we always try to
	     copy the file by hand if the rename failed.  */
	  errno = EXDEV;
#endif

	  if (errno != EXDEV)
	    {
	      ulog (LOG_ERROR, "rename (%s, %s): %s", zfrom, zto,
		    strerror (errno));
	      ubuffree (zfree);
	      break;
	    }

	  if (! fcopy_file (zfrom, zto, FALSE, FALSE, FALSE))
	    {
	      ubuffree (zfree);
	      break;
	    }
	  if (remove (zfrom) < 0)
	    ulog (LOG_ERROR, "remove (%s): %s", zfrom,
		  strerror (errno));
	}

      if (fto)
	(void) chmod (zto, IPUBLIC_FILE_MODE);

      ubuffree (zfree);
    }

  if (i < cfiles)
    {
      if (fto)
	(void) fsysdep_move_uuxqt_files (i, pzfrom, pzto, FALSE, iseq,
					 (char **) NULL);
      return FALSE;
    }

  return TRUE;
}
