/* log.c
   Routines to add entries to the log files.

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
const char log_rcsid[] = "$Id: log.c,v 1.1 1993/08/05 18:22:39 conklin Exp $";
#endif

#include <errno.h>

#if ANSI_C
#include <stdarg.h>
#endif

#if HAVE_TIME_H
#include <time.h>
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* Local functions.  */

static const char *zldate_and_time P((void));

/* Log file name.  */
static const char *zLogfile;

/* The function to call when a LOG_FATAL error occurs.  */
static void (*pfLfatal) P((void));

/* Whether to go to a file.  */
static boolean fLfile;

/* ID number.  */
static int iLid;

/* The current user name.  */
static char *zLuser;

/* The current system name.  */
static char *zLsystem;

/* The current device name.  */
char *zLdevice;

/* The open log file.  */
static FILE *eLlog;

/* Whether we have tried to open the log file.  We need this because
   we don't want to keep trying to open the log file if we failed the
   first time.  It can't be static because under HAVE_HDB_LOGGING we
   may have to write to various different log files.  */
static boolean fLlog_tried;

#if DEBUG > 1
/* Debugging file name.  */
static const char *zLdebugfile;

/* The open debugging file.  */
static FILE *eLdebug;

/* Whether we've tried to open the debugging file.  */
static boolean fLdebug_tried;

/* Whether we've written out any debugging information.  */
static boolean fLdebugging;
#endif

/* Statistics file name.  */
static const char *zLstatsfile;

/* The open statistics file.  */
static FILE *eLstats;

/* Whether we've tried to open the statistics file.  */
static boolean fLstats_tried;

/* The array of signals.  The elements are only set to TRUE by the
   default signal handler.  They are only set to FALSE if we don't
   care whether we got the signal or not.  */
volatile sig_atomic_t afSignal[INDEXSIG_COUNT];

/* The array of signals to log.  The elements are only set to TRUE by
   the default signal handler.  They are set to FALSE when the signal
   is logged in ulog.  This means that if a signal comes in at just
   the right time we won't log it (or, rather, we'll log it once
   instead of twice), but that is not a catatrophe.  */
volatile sig_atomic_t afLog_signal[INDEXSIG_COUNT];

/* Flag that indicates SIGHUP is worth logging.  */
boolean fLog_sighup = TRUE;

/* Signal names to use when logging signals.  */
static const char * const azSignal_names[INDEXSIG_COUNT] = INDEXSIG_NAMES;

/* If not NULL, ulog calls this function before outputting anything.
   This is used to support cu.  */
void (*pfLstart) P((void));

/* If not NULL, ulog calls this function after outputting everything.
   This is used to support cu.  */
void (*pfLend) P((void));

/* Set the function to call on a LOG_FATAL error.  */

void
ulog_fatal_fn (pfn)
     void (*pfn) P((void));
{
  pfLfatal = pfn;
}

/* Decide whether to send log message to the file or not.  */

void
ulog_to_file (puuconf, ffile)
     pointer puuconf;
     boolean ffile;
{
  int iuuconf;

  iuuconf = uuconf_logfile (puuconf, &zLogfile);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

#if DEBUG > 1
  iuuconf = uuconf_debugfile (puuconf, &zLdebugfile);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
#endif

  iuuconf = uuconf_statsfile (puuconf, &zLstatsfile);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  fLfile = ffile;
}

/* Set the ID number.  This will be called by the usysdep_initialize
   if there is something sensible to set it to.  */

void
ulog_id (i)
     int i;
{
  iLid = i;
}

/* Set the user we are making log entries for.  The arguments will be
   copied into memory.  */

void
ulog_user (zuser)
     const char *zuser;
{
  ubuffree (zLuser);
  zLuser = zbufcpy (zuser);
}

/* Set the system name we are making log entries for.  The name is copied
   into memory.  */

void
ulog_system (zsystem)
  const char *zsystem;
{
  if (zsystem == NULL
      || zLsystem == NULL
      || strcmp (zsystem, zLsystem) != 0)
    {
      ubuffree (zLsystem);
      zLsystem = zbufcpy (zsystem);
#if HAVE_HDB_LOGGING      
      /* Under HDB logging we now must write to a different log file.  */
      ulog_close ();
#endif /* HAVE_HDB_LOGGING */
    }
}

/* Set the device name.  This is copied into memory.  */

void
ulog_device (zdevice)
     const char *zdevice;
{
  ubuffree (zLdevice);
  zLdevice = zbufcpy (zdevice);
}

/* Make a log entry.  We make a token concession to non ANSI_C systems,
   but it clearly won't always work.  */

#if ! ANSI_C
#undef HAVE_VFPRINTF
#endif

/*VARARGS2*/
#if HAVE_VFPRINTF
void
ulog (enum tlog ttype, const char *zmsg, ...)
#else
void
ulog (ttype, zmsg, a, b, c, d, f, g, h, i, j)
     enum tlog ttype;
     const char *zmsg;
#endif
{
#if HAVE_VFPRINTF
  va_list parg;
#endif
  FILE *e, *edebug;
  boolean fstart, fend;
  const char *zhdr, *zstr;

  /* Log any received signal.  We do it this way to avoid calling ulog
     from the signal handler.  A few routines call ulog to get this
     message out with zmsg == NULL.  */
  {
    static boolean fdoing_sigs;

    if (! fdoing_sigs)
      {
	int isig;

	fdoing_sigs = TRUE;
	for (isig = 0; isig < INDEXSIG_COUNT; isig++)
	  {
	    if (afLog_signal[isig])
	      {
		afLog_signal[isig] = FALSE;

		/* Apparently SunOS sends SIGINT rather than SIGHUP
		   when hanging up, so we don't log either signal if
		   fLog_sighup is FALSE.  */
		if ((isig != INDEXSIG_SIGHUP && isig != INDEXSIG_SIGINT)
		    || fLog_sighup)
		  ulog (LOG_ERROR, "Got %s signal", azSignal_names[isig]);
	      }
	  }
	fdoing_sigs = FALSE;
      }
  }

  if (zmsg == NULL)
    return;

#if DEBUG > 1
  /* If we've had a debugging file open in the past, then we want to
     write all log file entries to the debugging file even if it's
     currently closed.  */
  if (fLfile
      && eLdebug == NULL
      && ! fLdebug_tried
      && (fLdebugging || (int) ttype >= (int) LOG_DEBUG))
    {
      fLdebug_tried = TRUE;
      eLdebug = esysdep_fopen (zLdebugfile, FALSE, TRUE, TRUE);
      fLdebugging = TRUE;
    }
#endif /* DEBUG > 1 */

  if (! fLfile)
    e = stderr;
#if DEBUG > 1
  else if ((int) ttype >= (int) LOG_DEBUG)
    {
      e = eLdebug;

      /* If we can't open the debugging file, don't output any
	 debugging messages.  */
      if (e == NULL)
	return;
    }
#endif /* DEBUG > 1 */
  else
    {
      if (eLlog == NULL && ! fLlog_tried)
	{
	  fLlog_tried = TRUE;
#if ! HAVE_HDB_LOGGING
	  eLlog = esysdep_fopen (zLogfile, TRUE, TRUE, TRUE);
#else /* HAVE_HDB_LOGGING */
	  {
	    const char *zsys;
	    char *zfile;

	    /* We want to write to .Log/program/system, e.g.  	
	       .Log/uucico/uunet.  The system name may not be set.  */
	    if (zLsystem == NULL)
	      zsys = "ANY";
	    else
	      zsys = zLsystem;

	    zfile = zbufalc (strlen (zLogfile)
			     + strlen (abProgram)
			     + strlen (zsys)
			     + 1);
	    sprintf (zfile, zLogfile, abProgram, zsys);
	    eLlog = esysdep_fopen (zfile, TRUE, TRUE, TRUE);
	    ubuffree (zfile);
	  }
#endif /* HAVE_HDB_LOGGING */

	  if (eLlog == NULL)
	    {
	      /* We can't open the log file.  We don't even have a
		 safe way to report this problem, since we may not be
		 able to write to stderr (it may, for example, be
		 attached to the incoming call).  */
	      if (pfLfatal != NULL)
		(*pfLfatal) ();
	      usysdep_exit (FALSE);
	    }
	}

      e = eLlog;

      /* eLlog might be NULL here because we might try to open the log
	 file recursively via esysdep_fopen.  */
      if (e == NULL)
	return;
    }

  if (pfLstart != NULL)
    (*pfLstart) ();

  edebug = NULL;
#if DEBUG > 1
  if ((int) ttype < (int) LOG_DEBUG)
    edebug = eLdebug;
#endif

  fstart = TRUE;
  fend = TRUE;

  switch (ttype)
    {
    case LOG_NORMAL:
      zhdr = "";
      break;
    case LOG_ERROR:
      zhdr = "ERROR: ";
      break;
    case LOG_FATAL:
      zhdr = "FATAL: ";
      break;
#if DEBUG > 1
    case LOG_DEBUG:
      zhdr = "DEBUG: ";
      break;
    case LOG_DEBUG_START:
      zhdr = "DEBUG: ";
      fend = FALSE;
      break;
    case LOG_DEBUG_CONTINUE:
      zhdr = NULL;
      fstart = FALSE;
      fend = FALSE;
      break;
    case LOG_DEBUG_END:
      zhdr = NULL;
      fstart = FALSE;
      break;
#endif
    default:
      zhdr = "???: ";
      break;
    }

  if (fstart)
    {
      if (! fLfile)
	{
	  fprintf (e, "%s: ", abProgram);
	  if (edebug != NULL)
	    fprintf (edebug, "%s: ", abProgram);
	}
      else
	{
#if HAVE_TAYLOR_LOGGING
	  fprintf (e, "%s ", abProgram);
	  if (edebug != NULL)
	    fprintf (edebug, "%s ", abProgram);
#else /* ! HAVE_TAYLOR_LOGGING */
	  fprintf (e, "%s ", zLuser == NULL ? "uucp" : zLuser);
	  if (edebug != NULL)
	    fprintf (edebug, "%s ", zLuser == NULL ? "uucp" : zLuser);
#endif /* HAVE_TAYLOR_LOGGING */

	  fprintf (e, "%s ", zLsystem == NULL ? "-" : zLsystem);
	  if (edebug != NULL)
	    fprintf (edebug, "%s ", zLsystem == NULL ? "-" : zLsystem);

#if HAVE_TAYLOR_LOGGING
	  fprintf (e, "%s ", zLuser == NULL ? "-" : zLuser);
	  if (edebug != NULL)
	    fprintf (edebug, "%s ", zLuser == NULL ? "-" : zLuser);
#endif /* HAVE_TAYLOR_LOGGING */

	  zstr = zldate_and_time ();
	  fprintf (e, "(%s", zstr);
	  if (edebug != NULL)
	    fprintf (edebug, "(%s", zstr); 

	  if (iLid != 0)
	    {
#if ! HAVE_HDB_LOGGING
#if HAVE_TAYLOR_LOGGING
	      fprintf (e, " %d", iLid);
	      if (edebug != NULL)
		fprintf (edebug, " %d", iLid);
#else /* ! HAVE_TAYLOR_LOGGING */
	      fprintf (e, "-%d", iLid);
	      if (edebug != NULL)
		fprintf (edebug, "-%d", iLid);
#endif /* ! HAVE_TAYLOR_LOGGING */
#else /* HAVE_HDB_LOGGING */

	      /* I assume that the second number here is meant to be
		 some sort of file sequence number, and that it should
		 correspond to the sequence number in the statistics
		 file.  I don't have any really convenient way to do
		 this, so I won't unless somebody thinks it's very
		 important.  */
	      fprintf (e, ",%d,%d", iLid, 0);
	      if (edebug != NULL)
		fprintf (edebug, ",%d,%d", iLid, 0);
#endif /* HAVE_HDB_LOGGING */
	    }

	  fprintf (e, ") ");
	  if (edebug != NULL)
	    fprintf (edebug, ") ");

	  fprintf (e, "%s", zhdr);
	  if (edebug != NULL)
	    fprintf (edebug, "%s", zhdr);
	}
    }

#if HAVE_VFPRINTF
  va_start (parg, zmsg);
  vfprintf (e, zmsg, parg);
  va_end (parg);
  if (edebug != NULL)
    {
      va_start (parg, zmsg);
      vfprintf (edebug, zmsg, parg);
      va_end (parg);
    }
#else /* ! HAVE_VFPRINTF */
  fprintf (e, zmsg, a, b, c, d, f, g, h, i, j);
  if (edebug != NULL)
    fprintf (edebug, zmsg, a, b, c, d, f, g, h, i, j);
#endif /* ! HAVE_VFPRINTF */

  if (fend)
    {
      fprintf (e, "\n");
      if (edebug != NULL)
	fprintf (edebug, "\n");
    }

  (void) fflush (e);
  if (edebug != NULL)
    (void) fflush (edebug);

  if (pfLend != NULL)
    (*pfLend) ();

  if (ttype == LOG_FATAL)
    {
      if (pfLfatal != NULL)
	(*pfLfatal) ();
      usysdep_exit (FALSE);
    }

#if CLOSE_LOGFILES
  ulog_close ();
#endif
}

/* Log a uuconf error.  */

void
ulog_uuconf (ttype, puuconf, iuuconf)
     enum tlog ttype;
     pointer puuconf;
     int iuuconf;
{
  char ab[512];

  (void) uuconf_error_string (puuconf, iuuconf, ab, sizeof ab);
  ulog (ttype, "%s", ab);
}

/* Close the log file.  There's nothing useful we can do with errors,
   so we don't check for them.  */

void
ulog_close ()
{
  /* Make sure we logged any signal we received.  */
  ulog (LOG_ERROR, (const char *) NULL);

  if (eLlog != NULL)
    {
      (void) fclose (eLlog);
      eLlog = NULL;
      fLlog_tried = FALSE;
    }

#if DEBUG > 1
  if (eLdebug != NULL)
    {
      (void) fclose (eLdebug);
      eLdebug = NULL;
      fLdebug_tried = FALSE;
    }
#endif
}

/* Add an entry to the statistics file.  We may eventually want to put
   failed file transfers in here, but we currently do not.  */

/*ARGSUSED*/
void
ustats (fsucceeded, zuser, zsystem, fsent, cbytes, csecs, cmicros, fmaster)
     boolean fsucceeded;
     const char *zuser;
     const char *zsystem;
     boolean fsent;
     long cbytes;
     long csecs;
     long cmicros;
     boolean fmaster;
{
  long cbps;

  /* The seconds and microseconds are now counted independently, so
     they may be out of synch.  */
  if (cmicros < 0)
    {
      csecs -= ((- cmicros) / 1000000L) + 1;
      cmicros = 1000000L - ((- cmicros) % 1000000L);
    }
  if (cmicros >= 1000000L)
    {
      csecs += cmicros / 10000000L;
      cmicros = cmicros % 1000000L;
    }      

  /* On a system which can determine microseconds we might very well
     have both csecs == 0 and cmicros == 0.  */
  if (csecs == 0 && cmicros < 1000)
    cbps = 0;
  else
    {
      long cmillis;

      /* This computation will not overflow provided csecs < 2147483
	 and cbytes and cbps both fit in a long.  */
      cmillis = csecs * 1000 + cmicros / 1000;
      cbps = ((cbytes / cmillis) * 1000
	      + ((cbytes % cmillis) * 1000) / cmillis);
    }

  if (eLstats == NULL)
    {
      if (fLstats_tried)
	return;
      fLstats_tried = TRUE;
      eLstats = esysdep_fopen (zLstatsfile, TRUE, TRUE, TRUE);
      if (eLstats == NULL)
	return;
    }

#if HAVE_TAYLOR_LOGGING
  fprintf (eLstats,
	   "%s %s (%s) %s%s %ld bytes in %ld.%03ld seconds (%ld bytes/sec)\n",
	   zuser, zsystem, zldate_and_time (),
	   fsucceeded ? "" : "failed after ",
	   fsent ? "sent" : "received",
	   cbytes, csecs, cmicros / 1000, cbps);
#endif /* HAVE_TAYLOR_LOGGING */
#if HAVE_V2_LOGGING
  fprintf (eLstats,
	   "%s %s (%s) (%ld) %s %s %ld bytes %ld seconds\n",
	   zuser, zsystem, zldate_and_time (),
	   (long) time ((time_t *) NULL),
	   fsent ? "sent" : "received",
	   fsucceeded ? "data" : "failed after",
	   cbytes, csecs + cmicros / 500000);
#endif /* HAVE_V2_LOGGING */
#if HAVE_HDB_LOGGING
  {
    static int iseq;

    /* I don't know what the 'C' means.  The sequence number should
       probably correspond to the sequence number in the log file, but
       that is currently always 0; using this fake sequence number
       will still at least reveal which transfers are from different
       calls.  We don't report a failed data transfer with this
       format.  */
    if (! fsucceeded)
      return;
    ++iseq;
    fprintf (eLstats,
	     "%s!%s %c (%s) (C,%d,%d) [%s] %s %ld / %ld.%03ld secs, %ld %s\n",
	     zsystem, zuser, fmaster ? 'M' : 'S', zldate_and_time (),
	     iLid, iseq, zLdevice == NULL ? "unknown" : zLdevice,
	     fsent ? "->" : "<-",
	     cbytes, csecs, cmicros / 1000, cbps,
	     "bytes/sec");
  }
#endif /* HAVE_HDB_LOGGING */

  (void) fflush (eLstats);

#if CLOSE_LOGFILES
  ustats_close ();
#endif
}

/* Close the statistics file.  */

void
ustats_close ()
{
  if (eLstats != NULL)
    {
      if (fclose (eLstats) != 0)
	ulog (LOG_ERROR, "fclose: %s", strerror (errno));
      eLstats = NULL;
      fLstats_tried = FALSE;
    }
}

/* Return the date and time in a form used for a log entry.  */

static const char *
zldate_and_time ()
{
  long isecs, imicros;
  struct tm s;
#if HAVE_TAYLOR_LOGGING
  static char ab[sizeof "1991-12-31 12:00:00.00"];
#endif
#if HAVE_V2_LOGGING
  static char ab[sizeof "12/31-12:00"];
#endif
#if HAVE_HDB_LOGGING
  static char ab[sizeof "12/31-12:00:00"];
#endif

  isecs = ixsysdep_time (&imicros);
  usysdep_localtime (isecs, &s);

#if HAVE_TAYLOR_LOGGING
  sprintf (ab, "%04d-%02d-%02d %02d:%02d:%02d.%02d",
	   s.tm_year + 1900, s.tm_mon + 1, s.tm_mday, s.tm_hour,
	   s.tm_min, s.tm_sec, (int) (imicros / 10000));
#endif
#if HAVE_V2_LOGGING
  sprintf (ab, "%d/%d-%02d:%02d", s.tm_mon + 1, s.tm_mday,
	   s.tm_hour, s.tm_min);
#endif
#if HAVE_HDB_LOGGING
  sprintf (ab, "%d/%d-%02d:%02d:%02d", s.tm_mon + 1, s.tm_mday,
	   s.tm_hour, s.tm_min, s.tm_sec);
#endif

  return ab;
}
