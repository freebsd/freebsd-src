/* spool.c
   Find a file in the spool directory.

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

#if USE_RCS_ID
const char spool_rcsid[] = "$Id: spool.c,v 1.2 1994/05/07 18:11:24 ache Exp $";
#endif

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* There are several types of files that go in the spool directory,
   and they go into various different subdirectories.  Whenever the
   system name LOCAL appears below, it means whatever the local system
   name is.

   Command files
   These contain instructions for uucico indicating what files to transfer
   to and from what systems.  Each line of a work file is a command
   beginning with S, R or X.
   #if ! SPOOLDIR_TAYLOR
   They are named C.ssssssgqqqq, where ssssss is the system name to
   transfer to or from, g is the grade and qqqq is the sequence number.
   #if SPOOLDIR_V2
   They are put in the spool directory.
   #elif SPOOLDIR_BSD42 || SPOOLDIR_BSD43
   They are put in the directory "C.".
   #elif SPOOLDIR_HDB
   They are put in a directory named for the system for which they were
   created.
   #elif SPOOLDIR_ULTRIX
   If the directory sys/ssssss exists, they are put in the directory
   sys/ssssss/C; otherwise, they are put in the directory sys/DEFAULT/C.
   #endif
   #elif SPOOLDIR_SVR4
   They are put in the directory sys/g, where sys is the system name
   and g is the grade.
   #endif
   #else SPOOLDIR_TAYLOR
   They are named C.gqqqq, where g is the grade and qqqq is the sequence
   number, and are placed in the directory ssssss/C. where ssssss is
   the system name to transfer to or from.
   #endif

   Data files
   There are files to be transferred to other systems.  Some files to
   be transferred may not be in the spool directory, depending on how
   uucp was invoked.  Data files are named in work files, so it is
   never necessary to look at them directly (except to remove old ones);
   it is only necessary to create them.  These means that the many
   variations in naming are inconsequential.
   #if ! SPOOLDIR_TAYLOR
   They are named D.ssssssgqqqq where ssssss is a system name (which
   may be LOCAL for locally initiated transfers or a remote system for
   remotely initiated transfers, except that HDB appears to use the
   system the file is being transferred to), g is the grade and qqqq
   is the sequence number.  Some systems use a trailing subjob ID
   number, but we currently do not.  The grade is not important, and
   some systems do not use it.  If the data file is to become an
   execution file on another system the grade (if present) will be
   'X'.  Otherwise Ultrix appears to use 'b'; the uux included with
   gnuucp 1.0 appears to use 'S'; SCO does not appear to use a grade,
   although it does use a subjob ID number.
   #if SPOOLDIR_V2
   They are put in the spool directory.
   #elif SPOOLDIR_BSD42
   If the name begins with D.LOCAL, the file is put in the directory
   D.LOCAL.  Otherwise the file is put in the directory D..
   #elif SPOOLDIR_BSD43
   If the name begins with D.LOCALX, the file is put in the directory
   D.LOCALX.  Otherwise if the name begins with D.LOCAL, the file is
   put in the directory D.LOCAL Otherwise the file is put in the
   directory "D.".
   #elif SPOOLDIR_HDB
   They are put in a directory named for the system for which they
   were created.
   #elif SPOOLDIR_ULTRIX
   Say the file is being transferred to system REMOTE.  If the
   directory sys/REMOTE exists, then if the file begins with D.LOCALX
   it is put in sys/REMOTE/D.LOCALX, if the file begins with D.LOCAL
   it is put in sys/REMOTE/D.LOCAL, and otherwise it is put in
   "sys/REMOTE/D.".  If the directory sys/REMOTE does not exist, the
   same applies except that DEFAULT is used instead of REMOTE.
   #elif SPOOLDIR_SVR4
   They are put in the directory sys/g, where sys is the system name
   and g is the grade.
   #endif
   #else SPOOLDIR_TAYLOR
   If the file is to become an executable file on another system it is
   named D.Xqqqq, otherwise it is named D.qqqq where in both cases
   qqqq is a sequence number.  If the corresponding C. file is in
   directory ssssss/C., a D.X file is placed in ssssss/D.X and a D.
   file is placed in "ssssss/D.".
   #endif

   Execute files
   These are files that specify programs to be executed.  They are
   created by uux, perhaps as run on another system.  These names are
   important, because a file transfer done to an execute file name
   causes an execution to occur.  The name is X.ssssssgqqqq, where
   ssssss is the requesting system, g is the grade, and qqqq is a
   sequence number.
   #if SPOOLDIR_V2 || SPOOLDIR_BSD42
   These files are placed in the spool directory.
   #elif SPOOLDIR_BSD43
   These files are placed in the directory X..
   #elif SPOOLDIR_HDB || SPOOLDIR_SVR4
   These files are put in a directory named for the system for which
   the files were created.
   #elif SPOOLDIR_ULTRIX
   If there is a spool directory (sys/ssssss) for the requesting
   system, the files are placed in sys/ssssss/X.; otherwise, the files
   are placed in "sys/DEFAULT/X.".
   #elif SPOOLDIR_TAYLOR
   The system name is automatically truncated to seven characters when
   a file is created.  The files are placed in the subdirectory X. of
   a directory named for the system for which the files were created.
   #endif

   Temporary receive files
   These are used when receiving files from another system.  They are
   later renamed to the final name.  The actual name is unimportant,
   although it generally begins with TM..
   #if SPOOLDIR_V2 || SPOOLDIR_BSD42
   These files are placed in the spool directory.
   #elif SPOOLDIR_BSD43 || SPOOLDIR_ULTRIX || SPOOLDIR_TAYLOR
   These files are placed in the directory .Temp.
   #elif SPOOLDIR_HDB || SPOOLDIR_SVR4
   These files are placed in a directory named for the system for
   which they were created.
   #endif

   System status files
   These are used to record when the last call was made to the system
   and what the status is.  They are used to prevent frequent recalls
   to a system which is not responding.  I will not attempt to
   recreate the format of these exactly, since they are not all that
   important.  They will be put in the directory .Status, as in HDB,
   and they use the system name as the name of the file.

   Sequence file
   This is used to generate a unique sequence number.  It contains an
   ASCII number.
   #if SPOOLDIR_V2 || SPOOLDIR_BSD42 || SPOOLDIR_BSD43
   The file is named SEQF and is kept in the spool directory.
   #elif SPOOLDIR_HDB || SPOOLDIR_SVR4
   A separate sequence file is kept for each system in the directory
   .Sequence with the name of the system.
   #elif SPOOLDIR_ULTRIX
   Each system with a file sys/ssssss has a sequence file in
   sys/ssssss/.SEQF.  Other systems use sys/DEFAULT/.SEQF.
   #else SPOOLDIR_TAYLOR
   A sequence file named SEQF is kept in the directory ssssss for each
   system.
   #endif
   */

/* Given the name of a file as specified in a UUCP command, and the
   system for which this file has been created, return where to find
   it in the spool directory.  The file will begin with C. (a command
   file), D. (a data file) or X. (an execution file).  Under
   SPOOLDIR_SVR4 we need to know the grade of the file created by the
   local system; this is the bgrade argument, which is -1 for a file
   from a remote system.  */

/*ARGSUSED*/
char *
zsfind_file (zsimple, zsystem, bgrade)
     const char *zsimple;
     const char *zsystem;
     int bgrade;
{
  /* zsysdep_spool_commands calls this with TMPXXX which we must treat
     as a C. file.  */
  if ((zsimple[0] != 'T'
       || zsimple[1] != 'M'
       || zsimple[2] != 'P')
      && ! fspool_file (zsimple))
    {
      ulog (LOG_ERROR, "Unrecognized file name %s", zsimple);
      return NULL;
    }

#if ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 && ! SPOOLDIR_TAYLOR
  if (*zsimple == 'X')
    {
      static char *zbuf;
      static size_t cbuf;
      size_t clen, cwant;

      /* Files beginning with X. are execute files.  It is important
	 for security reasons that we know the system which created
	 the X. file.  This is easy under SPOOLDIR_HDB or
	 SPOOLDIR_SVR4 SPOOLDIR_TAYLOR, because the file will be in a
	 directory named for the system.  Under other schemes, we must
	 get the system name from the X. file name.  To prevent
	 security violations, we set the system name directly here;
	 this will cause problems if the maximum file name length is
	 too short, but hopefully no problem will occur since any
	 System V systems will be using HDB or SVR4 or TAYLOR.  */
      clen = strlen (zsimple);
      if (clen < 5)
	{
	  ulog (LOG_ERROR, "Bad file name (too short) %s", zsimple);
	  return NULL;
	}
      cwant = strlen (zsystem) + 8;
      if (cwant > cbuf)
	{
	  zbuf = (char *) xrealloc ((pointer) zbuf, cwant);
	  cbuf = cwant;
	}
      sprintf (zbuf, "X.%s%s", zsystem, zsimple + clen - 5);
      zsimple = zbuf;
    }
#endif /* ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 && ! SPOOLDIR_TAYLOR */

#if SPOOLDIR_V2
  /* V2 never uses subdirectories.  */
  return zbufcpy (zsimple);
#endif /* SPOOLDIR_V2 */

#if SPOOLDIR_HDB
  /* HDB always uses the system name as a directory.  */
  return zsysdep_in_dir (zsystem, zsimple);
#endif /* SPOOLDIR_HDB */

#if SPOOLDIR_SVR4
  /* SVR4 uses grade directories within the system directory for local
     command and data files.  */
  if (bgrade < 0 || *zsimple == 'X')
    return zsysdep_in_dir (zsystem, zsimple);
  else
    {
      char abgrade[2];

      abgrade[0] = bgrade;
      abgrade[1] = '\0';
      return zsappend3 (zsystem, abgrade, zsimple);
    }
#endif /* SPOOLDIR_SVR4 */

#if ! SPOOLDIR_V2 && ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4
  switch (*zsimple)
    {
    case 'C':
    case 'T':
#if SPOOLDIR_BSD42 || SPOOLDIR_BSD43
      return zsysdep_in_dir ("C.", zsimple);
#endif /* SPOOLDIR_BSD42 || SPOOLDIR_BSD43 */
#if SPOOLDIR_ULTRIX
      if (fsultrix_has_spool (zsystem))
	return zsappend4 ("sys", zsystem, "C.", zsimple);
      else
	return zsappend4 ("sys", "DEFAULT", "C.", zsimple);
#endif /* SPOOLDIR_ULTRIX */
#if SPOOLDIR_TAYLOR
      return zsappend3 (zsystem, "C.", zsimple);
#endif /* SPOOLDIR_TAYLOR */

    case 'D':
#if SPOOLDIR_BSD42 || SPOOLDIR_BSD43
      {
	size_t c;
	boolean ftruncated;
      
	/* D.LOCAL in D.LOCAL/, others in D./.  If BSD43, D.LOCALX in
	   D.LOCALX/.  */
	ftruncated = TRUE;
	if (strncmp (zsimple + 2, zSlocalname, strlen (zSlocalname)) == 0)
	  {
	    c = strlen (zSlocalname);
	    ftruncated = FALSE;
	  }
	else if (strncmp (zsimple + 2, zSlocalname, 7) == 0)
	  c = 7;
	else if (strncmp (zsimple + 2, zSlocalname, 6) == 0)
	  c = 6;
	else
	  c = 0;
#if SPOOLDIR_BSD43
	if (c > 0 && zsimple[c + 2] == 'X')
	  c++;
#endif /* SPOOLDIR_BSD43 */
	if (c > 0)
	  {
	    char *zalloc;

	    zalloc = zbufalc (c + 3);
	    memcpy (zalloc, zsimple, c + 2);
	    zalloc[c + 2] = '\0';

	    /* If we truncated the system name, and there is no existing
	       directory with the truncated name, then just use D..  */
	    if (! ftruncated || fsysdep_directory (zalloc))
	      {
		char *zret;

		zret = zsysdep_in_dir (zalloc, zsimple);
		ubuffree (zalloc);
		return zret;
	      }
	    ubuffree (zalloc);
	  }
	return zsysdep_in_dir ("D.", zsimple);
      }
#endif /* SPOOLDIR_BSD42 || SPOOLDIR_BSD43 */
#if SPOOLDIR_ULTRIX
      {
	size_t c;
	boolean ftruncated;
	char *zfree;
	const char *zdir;
	char *zret;
      
	/* D.LOCALX in D.LOCALX/, D.LOCAL in D.LOCAL/, others in D./.  */
	ftruncated = TRUE;
	if (strncmp (zsimple + 2, zSlocalname, strlen (zSlocalname)) == 0)
	  {
	    c = strlen (zSlocalname);
	    ftruncated = FALSE;
	  }
	else if (strncmp (zsimple + 2, zSlocalname, 7) == 0)
	  c = 7;
	else if (strncmp (zsimple + 2, zSlocalname, 6) == 0)
	  c = 6;
	else
	  c = 0;
	if (c > 0 && zsimple[c + 2] == 'X')
	  ++c;
	if (c > 0)
	  {
	    zfree = zbufalc (c + 3);
	    memcpy (zfree, zsimple, c + 2);
	    zfree[c + 2] = '\0';
	    zdir = zfree;

	    /* If we truncated the name, and there is no directory for
	       the truncated name, then don't use it.  */
	    if (ftruncated)
	      {
		char *zlook;

		zlook = zsappend3 ("sys",
				   (fsultrix_has_spool (zsystem)
				    ? zsystem
				    : "DEFAULT"),
				   zdir);
		if (! fsysdep_directory (zlook))
		  zdir = "D.";
		ubuffree (zlook);
	      }
	  }
	else
	  {
	    zfree = NULL;
	    zdir = "D.";
	  }
      
	zret = zsappend4 ("sys",
			  (fsultrix_has_spool (zsystem)
			   ? zsystem
			   : "DEFAULT"),
			  zdir,
			  zsimple);
	ubuffree (zfree);
	return zret;
      }
#endif /* SPOOLDIR_ULTRIX */
#if SPOOLDIR_TAYLOR
      if (zsimple[2] == 'X')
	return zsappend3 (zsystem, "D.X", zsimple);
      else
	return zsappend3 (zsystem, "D.", zsimple);
#endif /* SPOOLDIR_TAYLOR */


    case 'X':
#if SPOOLDIR_BSD42
      return zbufcpy (zsimple);
#endif
#if SPOOLDIR_BSD43
      return zsysdep_in_dir ("X.", zsimple);
#endif
#if SPOOLDIR_ULTRIX
      return zsappend4 ("sys",
			(fsultrix_has_spool (zsystem)
			 ? zsystem
			 : "DEFAULT"),
			"X.",
			zsimple);
#endif
#if SPOOLDIR_TAYLOR
      return zsappend3 (zsystem, "X.", zsimple);
#endif
    }

  /* This is just to avoid warnings; it will never be executed.  */
  return NULL;
#endif /* ! SPOOLDIR_V2 && ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 */
}
