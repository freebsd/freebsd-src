/* lock.c
   Lock and unlock a file name.

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
const char lock_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>
#include <ctype.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#if TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#if HAVE_QNX_LOCKFILES
#include <sys/kernel.h>
#include <sys/psinfo.h>
#include <sys/seginfo.h>
#include <sys/vc.h>
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef localtime
extern struct tm *localtime ();
#endif

#if HAVE_QNX_LOCKFILES
static boolean fsqnx_stale P((unsigned long ipid, unsigned long inme,
			     unsigned long inid, boolean *pferr));
#endif

/* Lock something.  If the fspooldir argument is TRUE, the argument is
   a file name relative to the spool directory; otherwise the argument
   is a simple file name which should be created in the system lock
   directory (under HDB this is /etc/locks).  */

boolean
fsdo_lock (zlock, fspooldir, pferr)
     const char *zlock;
     boolean fspooldir;
     boolean *pferr;
{
  char *zfree;
  const char *zpath, *zslash;
  size_t cslash;
  pid_t ime;
  char *ztempfile;
  char abtempfile[sizeof "TMP12345678901234567890"];
  int o;
#if HAVE_QNX_LOCKFILES
  nid_t inme;
  char ab[23];
  char *zend;
#else
#if HAVE_V2_LOCKFILES
  int i;
#else
  char ab[12];
#endif
#endif
  int cwrote;
  const char *zerr;
  boolean fret;

  if (pferr != NULL)
    *pferr = TRUE;

  if (fspooldir)
    {
      zfree = NULL;
      zpath = zlock;
    }
  else
    {
      zfree = zsysdep_in_dir (zSlockdir, zlock);
      zpath = zfree;
    }

  ime = getpid ();
#if HAVE_QNX_LOCKFILES
  inme = getnid ();
#endif

  /* We do the actual lock by creating a file and then linking it to
     the final file name we want.  This avoids race conditions due to
     one process checking the file before we have finished writing it,
     and also works even if we are somehow running as root.

     First, create the file in the right directory (we must create the
     file in the same directory since otherwise we might attempt a
     cross-device link).  */
  zslash = strrchr (zpath, '/');
  if (zslash == NULL)
    cslash = 0;
  else
    cslash = zslash - zpath + 1;

#if HAVE_QNX_LOCKFILES
  sprintf (abtempfile, "TMP%010lx%010lx", (unsigned long) ime,
	   (unsigned long) inme);
#else
  sprintf (abtempfile, "TMP%010lx", (unsigned long) ime);
#endif
  ztempfile = zbufalc (cslash + sizeof abtempfile);
  memcpy (ztempfile, zpath, cslash);
  memcpy (ztempfile + cslash, abtempfile, sizeof abtempfile);

  o = creat (ztempfile, IPUBLIC_FILE_MODE);
  if (o < 0)
    {
      if (errno == ENOENT)
	{
	  if (! fsysdep_make_dirs (ztempfile, FALSE))
	    {
	      ubuffree (zfree);
	      ubuffree (ztempfile);
	      return FALSE;
	    }
	  o = creat (ztempfile, IPUBLIC_FILE_MODE);
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "creat (%s): %s", ztempfile, strerror (errno));
	  ubuffree (zfree);
	  ubuffree (ztempfile);
	  return FALSE;
	}
    }

#if HAVE_QNX_LOCKFILES
  sprintf (ab, "%10ld %10ld\n", (long) ime, (long) inme);
  cwrote = write (o, ab, strlen (ab));
#else
#if HAVE_V2_LOCKFILES
  i = (int) ime;
  cwrote = write (o, &i, sizeof i);
#else
  sprintf (ab, "%10ld\n", (long) ime);
  cwrote = write (o, ab, strlen (ab));
#endif
#endif

  zerr = NULL;
  if (cwrote < 0)
    zerr = "write";
  if (close (o) < 0)
    zerr = "close";
  if (zerr != NULL)
    {
      ulog (LOG_ERROR, "%s (%s): %s", zerr, ztempfile, strerror (errno));
      (void) remove (ztempfile);
      ubuffree (zfree);
      ubuffree (ztempfile);
      return FALSE;
    }

  /* Now try to link the file we just created to the lock file that we
     want.  If it fails, try reading the existing file to make sure
     the process that created it still exists.  We do this in a loop
     to make it easy to retry if the old locking process no longer
     exists.  */
  fret = TRUE;
  if (pferr != NULL)
    *pferr = FALSE;
  o = -1;
  zerr = NULL;

  while (link (ztempfile, zpath) != 0)
    {
      int cgot;
      pid_t ipid;
      boolean freadonly;
      struct stat st;
      char abtime[sizeof "1991-12-31 12:00:00"];
#if HAVE_QNX_LOCKFILES
      nid_t inid;
#endif

      fret = FALSE;

      if (errno != EEXIST)
	{
	  ulog (LOG_ERROR, "link (%s, %s): %s", ztempfile, zpath,
		strerror (errno));
	  if (pferr != NULL)
	    *pferr = TRUE;
	  break;
	}

      freadonly = FALSE;
      o = open ((char *) zpath, O_RDWR | O_NOCTTY, 0);
      if (o < 0)
	{
	  if (errno == EACCES)
	    {
	      freadonly = TRUE;
	      o = open ((char *) zpath, O_RDONLY, 0);
	    }
	  if (o < 0)
	    {
	      if (errno == ENOENT)
		{
		  /* The file was presumably removed between the link
		     and the open.  Try the link again.  */
		  fret = TRUE;
		  continue;
		}
	      zerr = "open";
	      break;
	    }
	}

      /* The race starts here.  See below for a discussion.  */

#if HAVE_V2_LOCKFILES
      cgot = read (o, &i, sizeof i);
#else
      cgot = read (o, ab, sizeof ab - 1);
#endif

      if (cgot < 0)
	{
	  zerr = "read";
	  break;
	}

#if DEBUG > 0
#if HAVE_V2_LOCKFILES
      {
	char ab[10];

	if (read (o, ab, sizeof ab) > 4
	    && isdigit (BUCHAR (ab[0])))
	  ulog (LOG_ERROR,
		"Lock file %s may be HDB format; check LOCKFILES in policy.h",
		zpath);
      }
#else
      if (cgot == 4)
	ulog (LOG_ERROR,
	      "Lock file %s may be V2 format; check LOCKFILES in policy.h",
	      zpath);
#endif
#endif /* DEBUG > 0 */

#if HAVE_QNX_LOCKFILES
      ab[cgot] = '\0';
      ipid = (pid_t) strtol (ab, &zend, 10);
      inid = (nid_t) strtol (zend, (char **) NULL, 10);
#else
#if HAVE_V2_LOCKFILES
      ipid = (pid_t) i;
#else
      ab[cgot] = '\0';
      ipid = (pid_t) strtol (ab, (char **) NULL, 10);
#endif
#endif

      /* On NFS, the link might have actually succeeded even though we
	 got a failure return.  This can happen if the original
	 acknowledgement was lost or delayed and the operation was
	 retried.  In this case the pid will be our own.  This
	 introduces a rather improbable race condition: if a stale
	 lock was left with our process ID in it, and another process
	 just did the kill, below, but has not yet changed the lock
	 file to hold its own process ID, we could start up and make
	 it all the way to here and think we have the lock.  I'm not
	 going to worry about this possibility.  */
      if (ipid == ime)
	{
#if HAVE_QNX_LOCKFILES
	  if (inid == inme)
#endif
	    {
	      fret = TRUE;
	      break;
	    }
	}

      /* If the lock file is empty (cgot == 0), we assume that it is
         stale.  This can happen if the system crashed after the lock
         file was created but before the process ID was written out.  */
      if (cgot > 0)
	{
#if HAVE_QNX_LOCKFILES
	  if (! fsqnx_stale ((unsigned long) ipid, (unsigned long) inme,
			     (unsigned long) inid, pferr))
	    break;
#else
	  /* If the process still exists, we will get EPERM rather
	     than ESRCH.  We then return FALSE to indicate that we
	     cannot make the lock.  */
	  if (kill (ipid, 0) == 0 || errno == EPERM)
	    break;
#endif
	}

      if (fstat (o, &st) < 0)
	strcpy (abtime, "unknown");
      else
	{
	  time_t itm;
	  struct tm *q;

	  itm = (time_t) st.st_mtime;
	  q = localtime (&itm);
	  sprintf (abtime, "%04d-%02d-%02d %02d:%02d:%02d",
		   q->tm_year + 1900, q->tm_mon + 1, q->tm_mday, q->tm_hour,
		   q->tm_min, q->tm_sec);
	}

#if HAVE_QNX_LOCKFILES
      ulog (LOG_ERROR,
	    "Stale lock %s held by process %ld on node %ld created %s",
	    zpath, (long) ipid, (long) inid, abtime);
#else
      ulog (LOG_ERROR, "Stale lock %s held by process %ld created %s",
	    zpath, (long) ipid, abtime);
#endif

      /* This is a stale lock, created by a process that no longer
	 exists.

	 Now we could remove the file (and, if the file mode disallows
	 writing, that's what we have to do), but we try to avoid
	 doing so since it causes a race condition.  If we remove the
	 file, and are interrupted any time after we do the read until
	 we do the remove, another process could get in, open the
	 file, find that it was a stale lock, remove the file and
	 create a new one.  When we regained control we would remove
	 the file the other process just created.

	 These files are being generated partially for the benefit of
	 cu, and it would be nice to avoid the race however cu avoids
	 it, so that the programs remain compatible.  Unfortunately,
	 nobody seems to know how cu avoids the race, or even if it
	 tries to avoid it at all.

	 There are a few ways to avoid the race.  We could use kernel
	 locking primitives, but they may not be available.  We could
	 link to a special file name, but if that file were left lying
	 around then no stale lock could ever be broken (Henry Spencer
	 would think this was a good thing).

	 Instead I've implemented the following procedure: seek to the
	 start of the file, write our pid into it, sleep for five
	 seconds, and then make sure our pid is still there.  Anybody
	 who checks the file while we're asleep will find our pid
	 there and fail the lock.  The only race will come from
	 another process which has done the read by the time we do our
	 write.  That process will then have five seconds to do its
	 own write.  When we wake up, we'll notice that our pid is no
	 longer in the file, and retry the lock from the beginning.

	 This relies on the atomicity of write(2).  If it possible for
	 the writes of two processes to be interleaved, the two
	 processes could livelock.  POSIX unfortunately leaves this
	 case explicitly undefined; however, given that the write is
	 of less than a disk block, it's difficult to imagine an
	 interleave occurring.

	 Note that this is still a race.  If it takes the second
	 process more than five seconds to do the kill, the lseek, and
	 the write, both processes will think they have the lock.
	 Perhaps the length of time to sleep should be configurable.
	 Even better, perhaps I should add a configuration option to
	 use a permanent lock file, which eliminates any race and
	 forces the installer to be aware of the existence of the
	 permanent lock file.

	 We stat the file after the sleep, to make sure some other
	 program hasn't deleted it for us.  */
      if (freadonly)
	{
	  (void) close (o);
	  o = -1;
	  (void) remove (zpath);
	  fret = TRUE;
	  continue;
	}

      if (lseek (o, (off_t) 0, SEEK_SET) != 0)
	{
	  zerr = "lseek";
	  break;
	}

#if HAVE_QNX_LOCKFILES
      sprintf (ab, "%10ld %10ld\n", (long) ime, (long) inme);
      cwrote = write (o, ab, strlen (ab));
#else
#if HAVE_V2_LOCKFILES
      i = (int) ime;
      cwrote = write (o, &i, sizeof i);
#else
      sprintf (ab, "%10ld\n", (long) ime);
      cwrote = write (o, ab, strlen (ab));
#endif
#endif

      if (cwrote < 0)
	{
	  zerr = "write";
	  break;
	}

      (void) sleep (5);

      if (lseek (o, (off_t) 0, SEEK_SET) != 0)
	{
	  zerr = "lseek";
	  break;
	}

#if HAVE_V2_LOCKFILES
      cgot = read (o, &i, sizeof i);
#else
      cgot = read (o, ab, sizeof ab - 1);
#endif

      if (cgot < 0)
	{
	  zerr = "read";
	  break;
	}

#if HAVE_QNX_LOCKFILES
      ab[cgot] = '\0';
      ipid = (pid_t) strtol (ab, &zend, 10);
      inid = (nid_t) strtol (zend, (char **) NULL, 10);
#else
#if HAVE_V2_LOCKFILES
      ipid = (pid_t) i;
#else
      ab[cgot] = '\0';
      ipid = (pid_t) strtol (ab, (char **) NULL, 10);
#endif
#endif

      if (ipid == ime)
	{
#if HAVE_QNX_LOCKFILES
	  if (inid == inme)
#endif
	    {
	      struct stat sfile, sdescriptor;

	      /* It looks like we have the lock.  Do the final stat
		 check.  */
	      if (stat ((char *) zpath, &sfile) < 0)
		{
		  if (errno != ENOENT)
		    {
		      zerr = "stat";
		      break;
		    }
		  /* Loop around and try again.  */
		}
	      else
		{
		  if (fstat (o, &sdescriptor) < 0)
		    {
		      zerr = "fstat";
		      break;
		    }

		  if (sfile.st_ino == sdescriptor.st_ino
		      && sfile.st_dev == sdescriptor.st_dev)
		    {
		      /* Close the file before assuming we've
			 succeeded to pick up any trailing errors.  */
		      if (close (o) < 0)
			{
			  zerr = "close";
			  break;
			}

		      o = -1;

		      /* We have the lock.  */
		      fret = TRUE;
		      break;
		    }
		}
	    }
	}

      /* Loop around and try the lock again.  We keep doing this until
	 the lock file holds a pid that exists.  */
      (void) close (o);
      o = -1;
      fret = TRUE;
    }

  if (zerr != NULL)
    {
      ulog (LOG_ERROR, "%s (%s): %s", zerr, zpath, strerror (errno));
      if (pferr != NULL)
	*pferr = TRUE;
    }

  if (o >= 0)
    (void) close (o);

  ubuffree (zfree);

  /* It would be nice if we could leave the temporary file around for
     future calls, but considering that we create lock files in
     various different directories it's probably more trouble than
     it's worth.  */
  if (remove (ztempfile) != 0)
    ulog (LOG_ERROR, "remove (%s): %s", ztempfile, strerror (errno));

  ubuffree (ztempfile);

  return fret;
}

/* Unlock something.  The fspooldir argument is as in fsdo_lock.  */

boolean
fsdo_unlock (zlock, fspooldir)
     const char *zlock;
     boolean fspooldir;
{
  char *zfree;
  const char *zpath;

  if (fspooldir)
    {
      zfree = NULL;
      zpath = zlock;
    }
  else
    {
      zfree = zsysdep_in_dir (zSlockdir, zlock);
      zpath = zfree;
    }

  if (remove (zpath) == 0
      || errno == ENOENT)
    {
      ubuffree (zfree);
      return TRUE;
    }
  else
    {
      ulog (LOG_ERROR, "remove (%s): %s", zpath, strerror (errno));
      ubuffree (zfree);
      return FALSE;
    }
}

#if HAVE_QNX_LOCKFILES

/* Return TRUE if the lock is stale.  */

static boolean
fsqnx_stale (ipid, inme, inid, pferr)
     unsigned long ipid;
     unsigned long inme;
     unsigned long inid;
     boolean *pferr;
{
  /* A virtual process ID.  This virtual process ID, which will exist
     on the local node, will represent the process ID of the process
     manager process (Proc) on the remote node. */
  pid_t ivid;
  /* The return value of the qnx_psinfo function.  This is either a
     process ID which might or might not be the same as the process
     being looked for, or -1 to indicate no process found. */
  pid_t ifound_pid;
  /* This holds the actual result of qnx_psinfo.  We will ignore
     almost all the fields since we're just checking for existence. */
  struct _psinfo spsdata;

  /* Establish connection with a remote process manager if necessary. */
  if (inid != inme)
    {
      ivid = qnx_vc_attach (inid /* remote node ID */,
			    PROC_PID /* pid of process manager */,
			    1000 /* initial buffer size */,
			    0	/* flags */);
      if (ivid < 0)
	{
	  ulog (LOG_ERROR, "qnx_vc_attach (%lu, PROC_PID): %s",
		inid, strerror (errno));
	  if (pferr != NULL)
	    *pferr = TRUE;
	  return FALSE;
	}
    }
  else
    {
      /* Use the local pid of the local process manager. */
      ivid = PROC_PID;
    }
        
  /* Request the process information. */
  ifound_pid = qnx_psinfo (ivid /* process manager handling request */,
			   ipid /* get info on this process */,
			   &spsdata /* put info in this struct */,
			   0	/* unused */,
			   (struct _seginfo *) NULL /* unused */);

  /* Deallocate the virtual connection before continuing. */
  {
    int isaved_errno = errno;
    if (qnx_vc_detach (ivid) < 0)
      ulog (LOG_ERROR, "qnx_vd_detach (%ld): %s", (long) ivid,
	    strerror (errno));
    errno = isaved_errno;
  }
          
  /* If the returned pid matches then the process still holds the lock. */
  if ((ifound_pid == ipid) && (spsdata.pid == ipid))
    return FALSE;

  /* If the returned pid is positive and doesn't match, then the
     process doesn't exist and the lock is stale.  Continue. */

  /* If the returned pid is negative (-1) and errno is EINVAL (or ESRCH
     in older versions of QNX), then the process doesn't exist and the
     lock is stale.  Continue. */

  /* Check for impossible errors. */
  if ((ifound_pid < 0) && (errno != ESRCH) && (errno != EINVAL))
    {
      ulog (LOG_ERROR, "qnx_psinfo (%ld, %ld): %s", (long) ivid,
	    (long) ipid, strerror (errno));
      /* Since we don't know what the hell this means, and we don't
	 want our system to freeze, we treat this case as a stale
	 lock.  Continue on. */
    }

  return TRUE;
}

#endif /* HAVE_QNX_LOCKFILES */
