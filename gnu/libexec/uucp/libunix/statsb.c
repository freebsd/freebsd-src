/* statsb.c
   System dependent routines for uustat.

   Copyright (C) 1992, 1993, 1994 Ian Lance Taylor

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

#if USE_RCS_ID
const char statsb_rcsid[] = "$Id: statsb.c,v 1.2 1994/05/07 18:11:29 ache Exp $";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

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

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

#if HAVE_TIME_H
#include <time.h>
#endif

#if HAVE_UTIME_H
#include <utime.h>
#endif

/* Local functions.  */

static int issettime P((const char *z, time_t inow));
static boolean fskill_or_rejuv P((pointer puuconf, const char *zid,
				  boolean fkill));

/* Set file access time to the present.  On many systems this could be
   done by passing NULL to utime, but on some that doesn't work.  This
   routine is not time critical, so we never rely on NULL.  */

static int
issettime(z, inow)
     const char *z;
     time_t inow;
{
#if HAVE_UTIME_H
  struct utimbuf s;

  s.actime = inow;
  s.modtime = inow;
  return utime ((char *) z, &s);
#else
  time_t ai[2];

  ai[0] = inow;
  ai[1] = inow;
  return utime ((char *) z, ai);
#endif
}

/* Kill a job, given the jobid.  */

boolean
fsysdep_kill_job (puuconf, zid)
     pointer puuconf;
     const char *zid;
{
  return fskill_or_rejuv (puuconf, zid, TRUE);
}

/* Rejuvenate a job, given the jobid.  */

boolean
fsysdep_rejuvenate_job (puuconf, zid)
     pointer puuconf;
     const char *zid;
{
  return fskill_or_rejuv (puuconf, zid, FALSE);
}

/* Kill or rejuvenate a job, given the jobid.  */

static boolean
fskill_or_rejuv (puuconf, zid, fkill)
     pointer puuconf;
     const char *zid;
     boolean fkill;
{
  char *zfile;
  char *zsys;
  char bgrade;
  time_t inow = 0;
  int iuuconf;
  struct uuconf_system ssys;
  FILE *e;
  boolean fret;
  char *zline;
  size_t cline;
  int isys;

  zfile = zsjobid_to_file (zid, &zsys, &bgrade);
  if (zfile == NULL)
    return FALSE;

  if (! fkill)
    inow = time ((time_t *) NULL);

  iuuconf = uuconf_system_info (puuconf, zsys, &ssys);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      if (! funknown_system (puuconf, zsys, &ssys))
	{
	  ulog (LOG_ERROR, "%s: Bad job id", zid);
	  ubuffree (zfile);
	  ubuffree (zsys);
	  return FALSE;
	}
    }
  else if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      ubuffree (zfile);
      ubuffree (zsys);
      return FALSE;
    }

  e = fopen (zfile, "r");
  if (e == NULL)
    {
      if (errno == ENOENT)
	ulog (LOG_ERROR, "%s: Job not found", zid);
      else
	ulog (LOG_ERROR, "fopen (%s): %s", zfile, strerror (errno));
      (void) uuconf_system_free (puuconf, &ssys);
      ubuffree (zfile);
      ubuffree (zsys);
      return FALSE;
    }

  /* Now we have to read through the file to identify any temporary
     files.  */
  fret = TRUE;
  zline = NULL;
  cline = 0;
  while (getline (&zline, &cline, e) > 0)
    {
      struct scmd s;

      if (! fparse_cmd (zline, &s))
	{
	  ulog (LOG_ERROR, "Bad line in command file %s", zfile);
	  fret = FALSE;
	  continue;
	}

      /* You are only permitted to delete a job if you submitted it or
	 if you are root or uucp.  */
      if (strcmp (s.zuser, zsysdep_login_name ()) != 0
	  && ! fsysdep_privileged ())
	{
	  ulog (LOG_ERROR, "%s: Not submitted by you", zid);
	  xfree ((pointer) zline);
	  (void) fclose (e);
	  (void) uuconf_system_free (puuconf, &ssys);
	  ubuffree (zfile);
	  ubuffree (zsys);
	  return FALSE;
	}

      if (s.bcmd == 'S' || s.bcmd == 'E')
	{
	  char *ztemp;

	  ztemp = zsfind_file (s.ztemp, ssys.uuconf_zname, bgrade);
	  if (ztemp == NULL)
	    fret = FALSE;
	  else
	    {
	      if (fkill)
		isys = remove (ztemp);
	      else
		isys = issettime (ztemp, inow);

	      if (isys != 0 && errno != ENOENT)
		{
		  ulog (LOG_ERROR, "%s (%s): %s",
			fkill ? "remove" : "utime", ztemp,
			strerror (errno));
		  fret = FALSE;
		}

	      ubuffree (ztemp);
	    }
	}
    }

  xfree ((pointer) zline);
  (void) fclose (e);
  (void) uuconf_system_free (puuconf, &ssys);
  ubuffree (zsys);

  if (fkill)
    isys = remove (zfile);
  else
    isys = issettime (zfile, inow);

  if (isys != 0 && errno != ENOENT)
    {
      ulog (LOG_ERROR, "%s (%s): %s", fkill ? "remove" : "utime",
	    zfile, strerror (errno));
      fret = FALSE;
    }

  ubuffree (zfile);

  return fret;
}

/* Get the time a work job was queued.  */

long
ixsysdep_work_time (qsys, pseq)
     const struct uuconf_system *qsys;
     pointer pseq;
{
  char *zjobid, *zfile;
  long iret;

  zjobid = zsysdep_jobid (qsys, pseq);
  zfile = zsjobid_to_file (zjobid, (char **) NULL, (char *) NULL);
  if (zfile == NULL)
    return 0;
  ubuffree (zjobid);
  iret = ixsysdep_file_time (zfile);
  ubuffree (zfile);
  return iret;
}

/* Get the time a file was created (actually, the time it was last
   modified).  */

long
ixsysdep_file_time (zfile)
     const char *zfile;
{
  struct stat s;

  if (stat ((char *) zfile, &s) < 0)
    {
      if (errno != ENOENT)
	ulog (LOG_ERROR, "stat (%s): %s", zfile, strerror (errno));
      return ixsysdep_time ((long *) NULL);
    }

  return (long) s.st_mtime;
}

/* Set the time of a file to the current time.  */

boolean
fsysdep_touch_file (zfile)
     const char *zfile;
{
  if (issettime (zfile, time ((time_t *) NULL)) != 0)
    {
      ulog (LOG_ERROR, "utime (%s): %s", zfile, strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* Start getting the status files.  */

boolean
fsysdep_all_status_init (phold)
     pointer *phold;
{
  DIR *qdir;

  qdir = opendir ((char *) ".Status");
  if (qdir == NULL)
    {
      ulog (LOG_ERROR, "opendir (.Status): %s", strerror (errno));
      return FALSE;
    }

  *phold = (pointer) qdir;
  return TRUE;
}

/* Get the next status file.  */

char *
zsysdep_all_status (phold, pferr, qstat)
     pointer phold;
     boolean *pferr;
     struct sstatus *qstat;
{
  DIR *qdir = (DIR *) phold;
  struct dirent *qentry;

  while (TRUE)
    {
      errno = 0;
      qentry = readdir (qdir);
      if (qentry == NULL)
	{
	  if (errno == 0)
	    *pferr = FALSE;
	  else
	    {
	      ulog (LOG_ERROR, "readdir: %s", strerror (errno));
	      *pferr = TRUE;
	    }
	  return NULL;
	}

      if (qentry->d_name[0] != '.')
	{
	  struct uuconf_system ssys;

	  /* Hack seriously; fsysdep_get_status only looks at the
	     zname element of the qsys argument, so if we fake that we
	     can read the status file.  This should really be done
	     differently.  */
	  ssys.uuconf_zname = qentry->d_name;
	  if (fsysdep_get_status (&ssys, qstat, (boolean *) NULL))
	    return zbufcpy (qentry->d_name);

	  /* If fsysdep_get_status fails, it will output an error
	     message.  We just continue with the next entry, so that
	     most of the status files will be displayed.  */
	}
    }
}

/* Finish getting the status file.  */

void
usysdep_all_status_free (phold)
     pointer phold;
{
  DIR *qdir = (DIR *) phold;

  (void) closedir (qdir);
}

/* Get the status of all processes holding lock files.  We do this by
   invoking ps after we've figured out the process entries to use.  */

boolean
fsysdep_lock_status ()
{
  DIR *qdir;
  struct dirent *qentry;
  int calc;
  pid_t *pai;
#if HAVE_QNX_LOCKFILES
  nid_t *painid;
#endif
  int cgot;
  int aidescs[3];
  char *zcopy, *ztok;
  int cargs, iarg;
  char **pazargs;

  qdir = opendir ((char *) zSlockdir);
  if (qdir == NULL)
    {
      ulog (LOG_ERROR, "opendir (%s): %s", zSlockdir, strerror (errno));
      return FALSE;
    }

  /* We look for entries that start with "LCK.." and ignore everything
     else.  This won't find all possible lock files, but it should
     find all the locks on terminals and systems.  */

  calc = 0;
  pai = NULL;
  cgot = 0;
#if HAVE_QNX_LOCKFILES
  painid = NULL;
#endif
  while ((qentry = readdir (qdir)) != NULL)
    {
      char *zname;
      int o;
#if HAVE_QNX_LOCKFILES
      nid_t inid;
      char ab[23];
      char *zend;
#else
#if HAVE_V2_LOCKFILES
      int i;
#else
      char ab[12];
#endif
#endif
      int cread;
      int ierr;
      pid_t ipid;
      int icheck;

      if (strncmp (qentry->d_name, "LCK..", sizeof "LCK.." - 1) != 0)
	continue;

      zname = zsysdep_in_dir (zSlockdir, qentry->d_name);
      o = open ((char *) zname, O_RDONLY | O_NOCTTY, 0);
      if (o < 0)
	{
	  if (errno != ENOENT)
	    ulog (LOG_ERROR, "open (%s): %s", zname, strerror (errno));
	  ubuffree (zname);
	  continue;
	}

#if HAVE_V2_LOCKFILES
      cread = read (o, &i, sizeof i);
#else
      cread = read (o, ab, sizeof ab - 1);
#endif

      ierr = errno;
      (void) close (o);

      if (cread < 0)
	{
	  ulog (LOG_ERROR, "read %s: %s", zname, strerror (ierr));
	  ubuffree (zname);
	  continue;
	}

      ubuffree (zname);

#if HAVE_QNX_LOCKFILES
      ab[cread] = '\0';
      ipid = (pid_t) strtol (ab, &zend, 10);
      inid = (nid_t) strtol (zend, (char **) NULL, 10);
#else
#if HAVE_V2_LOCKFILES
      ipid = (pid_t) i;
#else
      ab[cread] = '\0';
      ipid = (pid_t) strtol (ab, (char **) NULL, 10);
#endif
#endif

#if HAVE_QNX_LOCKFILES
      printf ("%s: %ld %ld\n", qentry->d_name, (long) inid, (long) ipid);
#else
      printf ("%s: %ld\n", qentry->d_name, (long) ipid);
#endif

      for (icheck = 0; icheck < cgot; icheck++)
	if (pai[icheck] == ipid)
	  break;
      if (icheck < cgot)
	continue;

      if (cgot >= calc)
	{
	  calc += 10;
	  pai = (pid_t *) xrealloc ((pointer) pai, calc * sizeof (pid_t));
#if HAVE_QNX_LOCKFILES
	  painid = (nid_t *) xrealloc ((pointer) painid,
				       calc * sizeof (nid_t));
#endif
	}

      pai[cgot] = ipid;
#if HAVE_QNX_LOCKFILES
      painid[cgot] = inid;
#endif
      ++cgot;
    }

  if (cgot == 0)
    return TRUE;

  aidescs[0] = SPAWN_NULL;
  aidescs[1] = 1;
  aidescs[2] = 2;

  /* Parse PS_PROGRAM into an array of arguments.  */
  zcopy = zbufcpy (PS_PROGRAM);

  cargs = 0;
  for (ztok = strtok (zcopy, " \t");
       ztok != NULL;
       ztok = strtok ((char *) NULL, " \t"))
    ++cargs;

  pazargs = (char **) xmalloc ((cargs + 1) * sizeof (char *));

  memcpy (zcopy, PS_PROGRAM, sizeof PS_PROGRAM);
  for (ztok = strtok (zcopy, " \t"), iarg = 0;
       ztok != NULL;
       ztok = strtok ((char *) NULL, " \t"), ++iarg)
    pazargs[iarg] = ztok;
  pazargs[iarg] = NULL;

#if ! HAVE_PS_MULTIPLE
  /* We have to invoke ps multiple times.  */
  {
    int i;
    char *zlast, *zset;
#if HAVE_QNX_LOCKFILES
    char *zpenultimate, *zsetnid;
#endif /* HAVE_QNX_LOCKFILES */

    zlast = pazargs[cargs - 1];
    zset = zbufalc (strlen (zlast) + 20);

#if HAVE_QNX_LOCKFILES
    /* We assume in this case that PS_PROGRAM ends with " -n -p".
       Thus, the last argument is "-p" and the second-to-last
       (penultimate) argument is "-n".  We modify them to read "-n###"
       and "-p###" where "###" is the node ID and the process ID,
       respectively.  This seems like quite a roundabout way of doing
       things.  Why don't we just leave the " -n -p" part out of
       PS_PROGRAM and construct the "-n###" and "-p###" arguments here
       from scratch?  Because that would not fit as well with how the
       code works for the other systems and would require larger
       changes. */
    zpenultimate = pazargs[cargs - 2];
    zsetnid = zbufalc (strlen (zpenultimate) + 20);
#endif

    for (i = 0; i < cgot; i++)
      {
	pid_t ipid;

	sprintf (zset, "%s%ld", zlast, (long) pai[i]);
	pazargs[cargs - 1] = zset;

#if HAVE_QNX_LOCKFILES
        sprintf (zsetnid, "%s%ld", zpenultimate, (long) painid[i]);
        pazargs[cargs - 2] = zsetnid;
#endif

	ipid = ixsspawn ((const char **) pazargs, aidescs, FALSE, FALSE,
			 (const char *) NULL, FALSE, TRUE,
			 (const char *) NULL, (const char *) NULL,
			 (const char *) NULL);
	if (ipid < 0)
	  ulog (LOG_ERROR, "ixsspawn: %s", strerror (errno));
	else
	  (void) ixswait ((unsigned long) ipid, PS_PROGRAM);
      }
    ubuffree (zset);
#if HAVE_QNX_LOCKFILES
    ubuffree (zsetnid);
#endif
  }
#else
  {
    char *zlast;
    int i;
    pid_t ipid;

    zlast = zbufalc (strlen (pazargs[cargs - 1]) + cgot * 20 + 1);
    strcpy (zlast, pazargs[cargs - 1]);
    for (i = 0; i < cgot; i++)
      {
	char ab[20];

	sprintf (ab, "%ld", (long) pai[i]);
	strcat (zlast, ab);
	if (i + 1 < cgot)
	  strcat (zlast, ",");
      }
    pazargs[cargs - 1] = zlast;

    ipid = ixsspawn ((const char **) pazargs, aidescs, FALSE, FALSE,
		     (const char *) NULL, FALSE, TRUE,
		     (const char *) NULL, (const char *) NULL,
		     (const char *) NULL);
    if (ipid < 0)
      ulog (LOG_ERROR, "ixsspawn: %s", strerror (errno));
    else
      (void) ixswait ((unsigned long) ipid, PS_PROGRAM);
    ubuffree (zlast);
  }
#endif

  ubuffree (zcopy);
  xfree ((pointer) pazargs);

  return TRUE;
}
