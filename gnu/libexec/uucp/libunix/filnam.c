/* filnam.c
   Get names to use for UUCP files.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

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

/* We need a definition for SEEK_SET.  */

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* External functions.  */
#ifndef lseek
extern off_t lseek ();
#endif

#define ZCHARS \
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* Local functions.  */

static boolean fscmd_seq P((const char *zsystem, char *zseq));
static char *zsfile_name P((int btype, const char *zsystem,
			    const char *zlocalname, int bgrade,
			    boolean fxqt, char *ztname, char *zdname,
			    char *zxname));

/* Get a new command sequence number (this is not a sequence number to
   be used for communicating with another system, but a sequence
   number to be used when generating the name of a command file).
   The sequence number is placed into zseq, which should be five
   characters long.  */

static boolean
fscmd_seq (zsystem, zseq)
     const char *zsystem;
     char *zseq;
{
  boolean ferr;
  char *zfree;
  const char *zfile;
  int o;
  int i;

  /* Lock the sequence file.  This may not be correct for all systems,
     but it only matters if the system UUCP and this UUCP are running
     at the same time.  */
  while (! fsdo_lock ("LCK..SEQ", TRUE, &ferr))
    {
      if (ferr || FGOT_SIGNAL ())
	return FALSE;
      sleep (5);
    }

  zfree = NULL;

#if SPOOLDIR_V2 || SPOOLDIR_BSD42 || SPOOLDIR_BSD43
  zfile = "SEQF";
#endif
#if SPOOLDIR_HDB || SPOOLDIR_SVR4
  zfree = zsysdep_in_dir (".Sequence", zsystem);
  zfile = zfree;
#endif
#if SPOOLDIR_ULTRIX
  if (! fsultrix_has_spool (zsystem))
    zfile = "sys/DEFAULT/.SEQF";
  else
    {
      zfree = zsappend3 ("sys", zsystem, ".SEQF");
      zfile = zfree;
    }
#endif /* SPOOLDIR_ULTRIX */
#if SPOOLDIR_TAYLOR
  zfree = zsysdep_in_dir (zsystem, "SEQF");
  zfile = zfree;
#endif /* SPOOLDIR_TAYLOR */

#ifdef O_CREAT
  o = open ((char *) zfile, O_RDWR | O_CREAT | O_NOCTTY, IPUBLIC_FILE_MODE);
#else
  o = open ((char *) zfile, O_RDWR | O_NOCTTY);
  if (o < 0 && errno == ENOENT)
    {
      o = creat ((char *) zfile, IPUBLIC_FILE_MODE);
      if (o >= 0)
	{
	  (void) close (o);
	  o = open ((char *) zfile, O_RDWR | O_NOCTTY);
	}
    }
#endif

  if (o < 0)
    {
      if (errno == ENOENT)
	{
	  if (! fsysdep_make_dirs (zfile, FALSE))
	    {
	      (void) fsdo_unlock ("LCK..SEQ", TRUE);
	      return FALSE;
	    }
#ifdef O_CREAT
	  o = open ((char *) zfile,
		    O_RDWR | O_CREAT | O_NOCTTY,
		    IPUBLIC_FILE_MODE);
#else
	  o = creat ((char *) zfile, IPUBLIC_FILE_MODE);
	  if (o >= 0)
	    {
	      (void) close (o);
	      o = open ((char *) zfile, O_RDWR | O_NOCTTY);
	    }
#endif
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "open (%s): %s", zfile, strerror (errno));
	  (void) fsdo_unlock ("LCK..SEQ", TRUE);
	  return FALSE;
	}
    }

  if (read (o, zseq, CSEQLEN) != CSEQLEN)
    strcpy (zseq, "0000");
  zseq[CSEQLEN] = '\0';

  /* We must add one to the sequence number and return the new value.
     On Ultrix, arbitrary characters are allowed in the sequence
     number.  On other systems, the sequence number apparently must be
     in hex.  */
#if SPOOLDIR_V2 || SPOOLDIR_BSD42 || SPOOLDIR_BSD43 || SPOOLDIR_HDB || SPOOLDIR_SVR4
  i = (int) strtol (zseq, (char **) NULL, 16);
  ++i;
  if (i > 0xffff)
    i = 0;
  /* The sprintf argument has CSEQLEN built into it.  */
  sprintf (zseq, "%04x", (unsigned int) i);
#endif
#if SPOOLDIR_ULTRIX || SPOOLDIR_TAYLOR
  for (i = CSEQLEN - 1; i >= 0; i--)
    {
      const char *zdig;

      zdig = strchr (ZCHARS, zseq[i]);
      if (zdig == NULL || zdig[0] == '\0' || zdig[1] == '\0')
	zseq[i] = '0';
      else
	{
	  zseq[i] = zdig[1];
	  break;
	}
    }
#endif /* SPOOLDIR_ULTRIX || SPOOLDIR_TAYLOR */

  if (lseek (o, (off_t) 0, SEEK_SET) < 0
      || write (o, zseq, CSEQLEN) != CSEQLEN
      || close (o) < 0)
    {
      ulog (LOG_ERROR, "lseek or write or close: %s", strerror (errno));
      (void) close (o);
      (void) fsdo_unlock ("LCK..SEQ", TRUE);
      return FALSE;
    }

  (void) fsdo_unlock ("LCK..SEQ", TRUE);

  return TRUE;
}

/* Get the name of a command or data file for a remote system.  The
   btype argument should be C for a command file or D for a data file.
   If the grade of a data file is X, it is assumed that this is going
   to become an execute file on some other system.  The zsystem
   argument is the system that the file will be transferred to.  The
   ztname argument will be set to a file name that could be passed to
   zsysdep_spool_file_name.  The zdname argument, if not NULL, will be
   set to a data file name appropriate for the remote system.  The
   zxname argument, if not NULL, will be set to the name of an execute
   file on the remote system.  None of the names will be more than 14
   characters long.  */

/*ARGSUSED*/
static char *
zsfile_name (btype, zsystem, zlocalname, bgrade, fxqt, ztname, zdname, zxname)
     int btype;
     const char *zsystem;
     const char *zlocalname;
     int bgrade;
     boolean fxqt;
     char *ztname;
     char *zdname;
     char *zxname;
{
  char abseq[CSEQLEN + 1];
  char absimple[11 + CSEQLEN];
  char *zname;

  if (zlocalname == NULL)
    zlocalname = zSlocalname;

  while (TRUE)
    {
      if (! fscmd_seq (zsystem, abseq))
	return NULL;

      if (btype == 'C')
	{
#if ! SPOOLDIR_TAYLOR
	  sprintf (absimple, "C.%.7s%c%s", zsystem, bgrade, abseq);
#else
	  sprintf (absimple, "C.%c%s", bgrade, abseq);
#endif
	}
      else if (btype == 'D')
	{
	  /* This name doesn't really matter that much; it's just the
	     name we use on the local system.  The name we use on the
	     remote system, which we return in zdname, should contain
	     our system name so that remote UUCP's running SPOOLDIR_V2
	     and the like can distinguish while files come from which
	     systems.  */
#if SPOOLDIR_HDB || SPOOLDIR_SVR4
	  sprintf (absimple, "D.%.7s%c%s", zsystem, bgrade, abseq);
#else /* ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 */
#if ! SPOOLDIR_TAYLOR
	  sprintf (absimple, "D.%.7s%c%s", zlocalname, bgrade, abseq);
#else /* SPOOLDIR_TAYLOR */
	  if (fxqt)
	    sprintf (absimple, "D.X%s", abseq);
	  else
	    sprintf (absimple, "D.%s", abseq);
#endif /* SPOOLDIR_TAYLOR */
#endif /* ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 */
	}
#if DEBUG > 0
      else
	ulog (LOG_FATAL, "zsfile_name: Can't happen");
#endif

      zname = zsfind_file (absimple, zsystem, bgrade);
      if (zname == NULL)
	return NULL;

      if (! fsysdep_file_exists (zname))
	break;

      ubuffree (zname);
    }

  if (ztname != NULL)
    strcpy (ztname, absimple);

  if (zdname != NULL)
    sprintf (zdname, "D.%.7s%c%s", zlocalname, bgrade, abseq);

  if (zxname != NULL)
    sprintf (zxname, "X.%.7s%c%s", zlocalname, bgrade, abseq);

  return zname;
}

/* Return a name to use for a data file to be copied to another
   system.  The name returned will be for a real file.  The zlocalname
   argument is the local name as seen by the remote system, the bgrade
   argument is the file grade, and the fxqt argument is TRUE if this
   file will become an execution file.  The ztname argument, if not
   NULL, will be set to a name that could be passed to
   zsysdep_spool_file_name to get back the return value of this
   function.  The zdname argument, if not NULL, will be set to a name
   that the file could be given on another system.  The zxname
   argument, if not NULL, will be set to a name for an execute file on
   another system.  */

char *
zsysdep_data_file_name (qsys, zlocalname, bgrade, fxqt, ztname, zdname,
			zxname)
     const struct uuconf_system *qsys;
     const char *zlocalname;
     int bgrade;
     boolean fxqt;
     char *ztname;
     char *zdname;
     char *zxname;
{
  return zsfile_name ('D', qsys->uuconf_zname, zlocalname, bgrade, fxqt, 
		      ztname, zdname, zxname);
}

/* Get a command file name.  */

char *
zscmd_file (qsys, bgrade)
     const struct uuconf_system *qsys;
     int bgrade;
{
  return zsfile_name ('C', qsys->uuconf_zname, (const char *) NULL,
		      bgrade, FALSE, (char *) NULL, (char *) NULL,
		      (char *) NULL);
}

/* Return a name for an execute file to be created locally.  This is
   used by uux to execute a command locally with remote files.  */

char *
zsysdep_xqt_file_name ()
{
  char abseq[CSEQLEN + 1];
  char absx[11 + CSEQLEN];
  char *zname;

  while (TRUE)
    {
      if (! fscmd_seq (zSlocalname, abseq))
	return NULL;

      sprintf (absx, "X.%.7sX%s", zSlocalname, abseq);

      zname = zsfind_file (absx, zSlocalname, -1);
      if (zname == NULL)
	return NULL;

      if (! fsysdep_file_exists (zname))
	break;

      ubuffree (zname);
    }

  return zname;
}
