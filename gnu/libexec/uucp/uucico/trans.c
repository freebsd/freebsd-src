/* trans.c
   Routines to handle file transfers.

   Copyright (C) 1992, 1993, 1995 Ian Lance Taylor

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
const char trans_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucico/trans.c,v 1.8 1999/08/27 23:33:51 peter Exp $";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "prot.h"
#include "system.h"
#include "trans.h"

/* Local functions.  */

static void utqueue P((struct stransfer **, struct stransfer *,
		       boolean fhead));
static void utdequeue P((struct stransfer *));
static void utchanalc P((struct sdaemon *qdaemon, struct stransfer *qtrans));
__inline__ static struct stransfer *qtchan P((int ichan));
__inline__ static void utchanfree P((struct stransfer *qtrans));
static boolean fttime P((struct sdaemon *qdaemon, long *pisecs,
			 long *pimicros));
static boolean fcheck_queue P((struct sdaemon *qdaemon));
static boolean ftadd_cmd P((struct sdaemon *qdaemon, const char *z,
			    size_t cdata, int iremote, boolean flast));
static boolean fremote_hangup_reply P((struct stransfer *qtrans,
				       struct sdaemon *qdaemon));
static boolean flocal_poll_file P((struct stransfer *qtrans,
				   struct sdaemon *qdaemon));

/* Queue of transfer structures that are ready to start which have
   been requested by the local system.  These are only permitted to
   start when the local system is the master.  */
static struct stransfer *qTlocal;

/* Queue of transfer structures that are ready to start which have
   been requested by the remote system.  These are responses to
   commands received from the remote system, and should be started as
   soon as possible.  */
static struct stransfer *qTremote;

/* Queue of transfer structures that have been started and want to
   send information.  This should be static, but the 'a' protocol
   looks at it, at least for now.  */
struct stransfer *qTsend;

/* Queue of transfer structures that have been started and are waiting
   to receive information.  */
static struct stransfer *qTreceive;

/* Queue of free transfer structures.  */
static struct stransfer *qTavail;

/* Array of transfer structures indexed by local channel number.  This
   is maintained for local jobs.  */
static struct stransfer *aqTchan[IMAX_CHAN + 1];

/* Number of local channel numbers currently allocated.  */
static int cTchans;

/* Next channel number to allocate.  */
static int iTchan;

/* Array of transfer structures indexed by remote channel number.
   This is maintained for remote jobs.  */
static struct stransfer *aqTremote[IMAX_CHAN + 1];

/* The transaction we are currently receiving.  This is used to avoid
   getting the time too frequently.  */
static struct stransfer *qTtiming_rec;

/* The time from which to charge any received data.  This is either
   the last time we charged for received data, or the last time
   something was put on the empty receive queue.  */
static long iTrecsecs;
static long iTrecmicros;

/* The minimum amount of time, in seconds, to wait between times we
   check the spool directory, if we are busy transferring data.  If we
   have nothing to do, we will check the spool directory regardless of
   how long ago the last check was.  This should probably be
   configurable.  */
#define CCHECKWAIT (600)

/* The time we last checked the spool directory for work.  This is set
   from the return value of ixsysdep_process_time, not ixsysdep_time,
   for convenience in the routines which use it.  */
static long iTchecktime;

/* The size of the command we have read so far in ftadd_cmd.  */
static size_t cTcmdlen;

/* The structure we use when waiting for an acknowledgement of a
   confirmed received file in fsent_receive_ack, and a list of those
   structures.  */

struct sreceive_ack
{
  struct sreceive_ack *qnext;
  char *zto;
  char *ztemp;
  boolean fmarked;
};

static struct sreceive_ack *qTreceive_ack;

/* Queue up a transfer structure before *pq.  This puts it at the head
   or the tail of the list headed by *pq.  */

static void
utqueue (pq, q, fhead)
     struct stransfer **pq;
     struct stransfer *q;
     boolean fhead;
{
  if (*pq == NULL)
    {
      *pq = q;
      q->qprev = q->qnext = q;
    }
  else
    {
      q->qnext = *pq;
      q->qprev = (*pq)->qprev;
      q->qprev->qnext = q;
      q->qnext->qprev = q;
      if (fhead)
	*pq = q;
    }
  q->pqqueue = pq;
}

/* Dequeue a transfer structure.  */

static void
utdequeue (q)
     struct stransfer *q;
{
  if (q->pqqueue != NULL)
    {
      if (*(q->pqqueue) == q)
	{
	  if (q->qnext == q)
	    *(q->pqqueue) = NULL;
	  else
	    *(q->pqqueue) = q->qnext;
	}
      q->pqqueue = NULL;
    }
  if (q->qprev != NULL)
    q->qprev->qnext = q->qnext;
  if (q->qnext != NULL)
    q->qnext->qprev = q->qprev;
  q->qprev = NULL;
  q->qnext = NULL;
}

/* Queue up a transfer structure requested by the local system.  */

/*ARGSIGNORED*/
boolean
fqueue_local (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  utdequeue (qtrans);
  utqueue (&qTlocal, qtrans, FALSE);
  return TRUE;
}

/* Queue up a transfer structure requested by the remote system.  The
   stransfer structure should have the iremote field set.  We need to
   record it, so that any subsequent data associated with this
   channel can be routed to the right place.  */

boolean
fqueue_remote (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fqueue_remote: Channel %d",
		  qtrans->iremote);
  if (qtrans->iremote > 0)
    aqTremote[qtrans->iremote] = qtrans;
  utdequeue (qtrans);
  utqueue (&qTremote, qtrans, FALSE);
  return TRUE;
}

/* Queue up a transfer with something to send.  */

boolean
fqueue_send (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
#if DEBUG > 0
  if (qtrans->psendfn == NULL)
    ulog (LOG_FATAL, "fqueue_send: Bad call");
#endif
  utdequeue (qtrans);

  /* Sort the send queue to always send commands before files, and to
     sort jobs by grade.  */
  if (qTsend == NULL)
    utqueue (&qTsend, qtrans, FALSE);
  else
    {
      register struct stransfer *q;
      boolean ffirst;

      ffirst = TRUE;
      q = qTsend;
      do
	{
	  if (! qtrans->fsendfile && q->fsendfile)
	    break;
	  if ((! qtrans->fsendfile || q->fsendfile)
	      && UUCONF_GRADE_CMP (qtrans->s.bgrade, q->s.bgrade) < 0)
	    break;

	  ffirst = FALSE;
	  q = q->qnext;
	}
      while (q != qTsend);

      qtrans->qnext = q;
      qtrans->qprev = q->qprev;
      q->qprev = qtrans;
      qtrans->qprev->qnext = qtrans;
      if (ffirst)
	qTsend = qtrans;
      qtrans->pqqueue = &qTsend;
    }

  return TRUE;
}

/* Queue up a transfer with something to receive.  */

boolean
fqueue_receive (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
#if DEBUG > 0
  if (qtrans->precfn == NULL)
    ulog (LOG_FATAL, "fqueue_receive: Bad call");
#endif

  /* If this is the only item on the receive queue, we do not want to
     charge it for any time during which we have not been waiting for
     anything, so update the receive timestamp.  */
  if (qTreceive == NULL)
    iTrecsecs = ixsysdep_process_time (&iTrecmicros);

  utdequeue (qtrans);
  utqueue (&qTreceive, qtrans, FALSE);

  return TRUE;
}

/* Get a new local channel number.  */

static void
utchanalc (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  do
    {
      ++iTchan;
      if (iTchan > qdaemon->cchans)
	iTchan = 1;
    }
  while (aqTchan[iTchan] != NULL);

  qtrans->ilocal = iTchan;
  aqTchan[iTchan] = qtrans;
  ++cTchans;
}

/* Return the transfer for a channel number.  */

__inline__
static struct stransfer *
qtchan (ic)
     int ic;
{
  return aqTchan[ic];
}

/* Clear the channel number for a transfer.  */

__inline__
static void
utchanfree (qt)
     struct stransfer *qt;
{
  if (qt->ilocal != 0)
    {
      aqTchan[qt->ilocal] = NULL;
      qt->ilocal = 0;
      --cTchans;
    }
}

/* Allocate a new transfer structure.  */

struct stransfer *
qtransalc (qcmd)
     struct scmd *qcmd;
{
  register struct stransfer *q;

  q = qTavail;
  if (q != NULL)
    utdequeue (q);
  else
    q = (struct stransfer *) xmalloc (sizeof (struct stransfer));
  q->qnext = NULL;
  q->qprev = NULL;
  q->pqqueue = NULL;
  q->psendfn = NULL;
  q->precfn = NULL;
  q->pinfo = NULL;
  q->fsendfile = FALSE;
  q->frecfile = FALSE;
  q->e = EFILECLOSED;
  q->ipos = 0;
  q->fcmd = FALSE;
  q->zcmd = NULL;
  q->ccmd = 0;
  q->ilocal = 0;
  q->iremote = 0;
  if (qcmd != NULL)
    {
      q->s = *qcmd;
      q->s.zfrom = zbufcpy (qcmd->zfrom);
      q->s.zto = zbufcpy (qcmd->zto);
      q->s.zuser = zbufcpy (qcmd->zuser);
      q->s.zoptions = zbufcpy (qcmd->zoptions);
      q->s.ztemp = zbufcpy (qcmd->ztemp);
      q->s.znotify = zbufcpy (qcmd->znotify);
      q->s.zcmd = zbufcpy (qcmd->zcmd);
    }
  else
    {
      q->s.zfrom = NULL;
      q->s.zto = NULL;
      q->s.zuser = NULL;
      q->s.zoptions = NULL;
      q->s.ztemp = NULL;
      q->s.znotify = NULL;
      q->s.zcmd = NULL;
    }
  q->zlog = NULL;
  q->isecs = 0;
  q->imicros = 0;
  q->cbytes = 0;

  return q;
}

/* Free a transfer structure.  This does not free any pinfo
   information that may have been allocated.  */

void
utransfree (q)
     struct stransfer *q;
{
  ubuffree (q->zcmd);
  ubuffree ((char *) q->s.zfrom);
  ubuffree ((char *) q->s.zto);
  ubuffree ((char *) q->s.zuser);
  ubuffree ((char *) q->s.zoptions);
  ubuffree ((char *) q->s.ztemp);
  ubuffree ((char *) q->s.znotify);
  ubuffree ((char *) q->s.zcmd);
  
  utchanfree (q);    
  if (q->iremote > 0)
    {
      aqTremote[q->iremote] = NULL;
      q->iremote = 0;
    }

#if DEBUG > 0
  q->e = EFILECLOSED;
  q->zcmd = NULL;
  q->s.zfrom = NULL;
  q->s.zto = NULL;
  q->s.zuser = NULL;
  q->s.zoptions = NULL;
  q->s.ztemp = NULL;
  q->s.znotify = NULL;
  q->s.zcmd = NULL;
  q->psendfn = NULL;
  q->precfn = NULL;
#endif

  /* Avoid any possible confusion in the timing code.  */
  if (qTtiming_rec == q)
    qTtiming_rec = NULL;

  utdequeue (q);
  utqueue (&qTavail, q, FALSE);
}

/* Get the time.  This is a wrapper around ixsysdep_process_time.  If
   enough time has elapsed since the last time we got the time, check
   the work queue.  */

static boolean
fttime (qdaemon, pisecs, pimicros)
     struct sdaemon *qdaemon;
     long *pisecs;
     long *pimicros;
{
  *pisecs = ixsysdep_process_time (pimicros);
  if (*pisecs - iTchecktime >= CCHECKWAIT)
    {
      if (! fcheck_queue (qdaemon))
	return FALSE;
    }
  return TRUE;
}

/* Gather local commands and queue them up for later processing.  Also
   recompute time based control values.  */

boolean
fqueue (qdaemon, pfany)
     struct sdaemon *qdaemon;
     boolean *pfany;
{
  const struct uuconf_system *qsys;
  long ival;
  int bgrade;
  struct uuconf_timespan *qlocal_size, *qremote_size;

  if (pfany != NULL)
    *pfany = FALSE;

  qsys = qdaemon->qsys;

  /* If we are not the caller, the grade will be set during the
     initial handshake, although this may be overridden by the
     calledtimegrade configuration option.  */
  if (! qdaemon->fcaller)
    {
      if (! ftimespan_match (qsys->uuconf_qcalledtimegrade, &ival,
			     (int *) NULL))
	bgrade = qdaemon->bgrade;
      else
	bgrade = (char) ival;
    }
  else
    {
      if (! ftimespan_match (qsys->uuconf_qtimegrade, &ival,
			     (int *) NULL))
	bgrade = '\0';
      else
	bgrade = (char) ival;
    }

  /* Determine the maximum sizes we can send and receive.  */
  if (qdaemon->fcaller)
    {
      qlocal_size = qsys->uuconf_qcall_local_size;
      qremote_size = qsys->uuconf_qcall_remote_size;
    }
  else
    {
      qlocal_size = qsys->uuconf_qcalled_local_size;
      qremote_size = qsys->uuconf_qcalled_remote_size;
    }

  if (! ftimespan_match (qlocal_size, &qdaemon->clocal_size, (int *) NULL))
    qdaemon->clocal_size = (long) -1;
  if (! ftimespan_match (qremote_size, &qdaemon->cremote_size, (int *) NULL))
    qdaemon->cremote_size = (long) -1;

  if (bgrade == '\0')
    return TRUE;

  if (! fsysdep_get_work_init (qsys, bgrade))
    return FALSE;

  while (TRUE)
    {
      struct scmd s;

      if (! fsysdep_get_work (qsys, bgrade, &s))
	return FALSE;

      if (s.bcmd == 'H')
	{
	  ulog_user ((const char *) NULL);
	  break;
	}

      if (s.bcmd == 'P')
	{
	  struct stransfer *qtrans;

	  /* A poll file.  */
	  ulog_user ((const char *) NULL);
	  qtrans = qtransalc (&s);
	  qtrans->psendfn = flocal_poll_file;
	  if (! fqueue_local (qdaemon, qtrans))
	    return FALSE;
	  continue;
	}

      ulog_user (s.zuser);

      switch (s.bcmd)
	{
	case 'S':
	case 'E':
	  if (! flocal_send_file_init (qdaemon, &s))
	    return FALSE;
	  break;
	case 'R':
	  if (! flocal_rec_file_init (qdaemon, &s))
	    return FALSE;
	  break;
	case 'X':
	  if (! flocal_xcmd_init (qdaemon, &s))
	    return FALSE;
	  break;
#if DEBUG > 0
	default:
	  ulog (LOG_FATAL, "fqueue: Can't happen");
	  break;
#endif
	}
    }	  

  if (pfany != NULL)
    *pfany = qTlocal != NULL;

  iTchecktime = ixsysdep_process_time ((long *) NULL);

  return TRUE;
}

/* Clear everything off the work queue.  This is used when the call is
   complete, or if the call is never made.  */

void
uclear_queue (qdaemon)
     struct sdaemon *qdaemon;
{
  int i;

  usysdep_get_work_free (qdaemon->qsys);

  qTlocal = NULL;
  qTremote = NULL;
  qTsend = NULL;
  qTreceive = NULL;
  cTchans = 0;
  iTchan = 0;
  qTtiming_rec = NULL;
  cTcmdlen = 0;
  qTreceive_ack = NULL;
  for (i = 0; i < IMAX_CHAN + 1; i++)
    {
      aqTchan[i] = NULL;
      aqTremote[i] = NULL;
    }
}

/* Recheck the work queue during a conversation.  This is only called
   if it's been more than CCHECKWAIT seconds since the last time the
   queue was checked.  */

static boolean
fcheck_queue (qdaemon)
     struct sdaemon *qdaemon;
{
  /* Only check if we are the master, or if there are multiple
     channels, or if we aren't already trying to get the other side to
     hang up.  Otherwise, there's nothing we can do with any new jobs
     we might find.  */
  if (qdaemon->fmaster
      || qdaemon->cchans > 1
      || ! qdaemon->frequest_hangup)
    {
      boolean fany;

      DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO,
		      "fcheck_queue: Rechecking work queue");
      if (! fqueue (qdaemon, &fany))
	return FALSE;

      /* If we found something to do, and we're not the master, and we
	 don't have multiple channels to send new jobs over, try to
	 get the other side to hang up.  */
      if (fany && ! qdaemon->fmaster && qdaemon->cchans <= 1)
	qdaemon->frequest_hangup = TRUE;
    }

  return TRUE;
}

/* The main transfer loop.  The uucico daemon spends essentially all
   its time in this function.  */

boolean
floop (qdaemon)
     struct sdaemon *qdaemon;
{
  boolean fret;

  fret = TRUE;

  while (! qdaemon->fhangup)
    {
      register struct stransfer *q;

#if DEBUG > 1
      /* If we're doing any debugging, close the log and debugging
	 files regularly.  This will let people copy them off and
	 remove them while the conversation is in progresss.  */
      if (iDebug != 0)
	{
	  ulog_close ();
	  ustats_close ();
	}
#endif

      if (qdaemon->fmaster)
	{
	  boolean fhangup;

	  /* We've managed to become the master, so we no longer want
	     to request a hangup.  */
	  qdaemon->frequest_hangup = FALSE;

	  fhangup = FALSE;

	  if (qdaemon->fhangup_requested
	      && qTsend == NULL)
	    {
	      /* The remote system has requested that we transfer
		 control by sending CYM after receiving a file.  */
	      DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO,
			      "floop: Transferring control at remote request");
	      fhangup = TRUE;
	    }
	  else if (qTremote == NULL
		   && qTlocal == NULL
		   && qTsend == NULL
		   && qTreceive == NULL)
	    {
	      /* We don't have anything to do.  Try to find some new
		 jobs.  If we can't, transfer control.  */
	      if (! fqueue (qdaemon, (boolean *) NULL))
		{
		  fret = FALSE;
		  break;
		}
	      if (qTlocal == NULL)
		{
		  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO,
				  "floop: No work for master");
		  fhangup = TRUE;
		}
	    }

	  if (fhangup)
	    {
	      if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, "H", 0, 0))
		{
		  fret = FALSE;
		  break;
		}
	      qdaemon->fmaster = FALSE;
	    }
	}

      /* If we are no long the master, clear any requested hangup.  We
	 may have already hung up before checking this variable in the
	 block above.  */
      if (! qdaemon->fmaster)
	qdaemon->fhangup_requested = FALSE;

      /* Immediately queue up any remote jobs.  We don't need local
	 channel numbers for them, since we can disambiguate based on
	 the remote channel number.  */
      while (qTremote != NULL)
	{
	  q = qTremote;
	  utdequeue (q);
	  utqueue (&qTsend, q, TRUE);
	}

      /* If we are the master, or if we have multiple channels, try to
	 queue up additional local jobs.  */
      if (qdaemon->fmaster || qdaemon->cchans > 1)
	{
	  while (qTlocal != NULL && cTchans < qdaemon->cchans)
	    {
	      /* We have room for an additional channel.  */
	      q = qTlocal;
	      if (! fqueue_send (qdaemon, q))
		{
		  fret = FALSE;
		  break;
		}
	      utchanalc (qdaemon, q);
	    }
	  if (! fret)
	    break;
	}

      q = qTsend;

      if (q == NULL)
	{
	  ulog_user ((const char *) NULL);
	  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "floop: Waiting for data");
	  if (! (*qdaemon->qproto->pfwait) (qdaemon))
	    {
	      fret = FALSE;
	      break;
	    }
	}
      else
	{
	  ulog_user (q->s.zuser);

	  if (! q->fsendfile)
	    {
	      /* Technically, we should add the time required for this
                 call to q->isecs and q->imicros.  In practice, the
                 amount of time required should be sufficiently small
                 that it can be safely disregarded.  */
	      if (! (*q->psendfn) (q, qdaemon))
		{
		  fret = FALSE;
		  break;
		}
	    }
	  else
	    {
	      long isecs, imicros;
	      boolean fcharged;
	      long inextsecs = 0, inextmicros;

	      if (! fttime (qdaemon, &isecs, &imicros))
		{
		  fret = FALSE;
		  break;
		}
	      fcharged = FALSE;

	      if (q->zlog != NULL)
		{
		  ulog (LOG_NORMAL, "%s", q->zlog);
		  ubuffree (q->zlog);
		  q->zlog = NULL;
		}

	      /* We can read the file in a tight loop until we have a
		 command to send, or the file send has been cancelled,
		 or we have a remote job to deal with.  We can
		 disregard any changes to qTlocal since we already
		 have something to send anyhow.  */
	      while (q == qTsend
		     && q->fsendfile
		     && qTremote == NULL)
		{
		  char *zdata;
		  size_t cdata;
		  long ipos;

		  zdata = (*qdaemon->qproto->pzgetspace) (qdaemon, &cdata);
		  if (zdata == NULL)
		    {
		      fret = FALSE;
		      break;
		    }

		  if (ffileeof (q->e))
		    cdata = 0;
		  else
		    {
		      cdata = cfileread (q->e, zdata, cdata);
		      if (ffileioerror (q->e, cdata))
			{
			  /* There is no way to report a file reading
			     error, so we just drop the connection.  */
			  ulog (LOG_ERROR, "read: %s", strerror (errno));
			  fret = FALSE;
			  break;
			}
		    }

		  ipos = q->ipos;
		  q->ipos += cdata;
		  q->cbytes += cdata;

		  if (! (*qdaemon->qproto->pfsenddata) (qdaemon, zdata,
							cdata, q->ilocal,
							q->iremote, ipos))
		    {
		      fret = FALSE;
		      break;
		    }

		  if (cdata == 0)
		    {
		      /* We must update the time now, because this
			 call may make an entry in the statistics
			 file.  */
		      inextsecs = ixsysdep_process_time (&inextmicros);
		      DEBUG_MESSAGE4 (DEBUG_UUCP_PROTO,
				      "floop: Charging %ld to %c %s %s",
				      ((inextsecs - isecs) * 1000000
				       + inextmicros - imicros),
				      q->s.bcmd, q->s.zfrom, q->s.zto);
		      q->isecs += inextsecs - isecs;
		      q->imicros += inextmicros - imicros;
		      fcharged = TRUE;

		      q->fsendfile = FALSE;

		      if (! (*q->psendfn) (q, qdaemon))
			fret = FALSE;

		      break;
		    }
		}

	      if (! fret)
		break;

	      if (! fcharged)
		{
		  inextsecs = ixsysdep_process_time (&inextmicros);
		  DEBUG_MESSAGE4 (DEBUG_UUCP_PROTO,
				  "floop: Charging %ld to %c %s %s",
				  ((inextsecs - isecs) * 1000000
				   + inextmicros - imicros),
				  q->s.bcmd, q->s.zfrom, q->s.zto);
		  q->isecs += inextsecs - isecs;
		  q->imicros += inextmicros - imicros;
		}

	      if (inextsecs - iTchecktime >= CCHECKWAIT)
		{
		  if (! fcheck_queue (qdaemon))
		    {
		      fret = FALSE;
		      break;
		    }
		}
	    }
	}
    }

  ulog_user ((const char *) NULL);

  (void) (*qdaemon->qproto->pfshutdown) (qdaemon);

  if (fret)
    uwindow_acked (qdaemon, TRUE);
  else
    ufailed (qdaemon);

  return fret;
}

/* This is called by the protocol routines when they have received
   some data.  If pfexit is not NULL, *pfexit should be set to TRUE if
   the protocol receive loop should exit back to the main floop
   routine, above.  It is only important to set *pfexit to TRUE if the
   main loop called the pfwait entry point, so we need never set it to
   TRUE if we just receive data for a file.  This routine never sets
   *pfexit to FALSE.  */

boolean 
fgot_data (qdaemon, zfirst, cfirst, zsecond, csecond, ilocal, iremote, ipos,
	   fallacked, pfexit)
     struct sdaemon *qdaemon;
     const char *zfirst;
     size_t cfirst;
     const char *zsecond;
     size_t csecond;
     int ilocal;
     int iremote;
     long ipos;
     boolean fallacked;
     boolean *pfexit;
{
  struct stransfer *q;
  int cwrote;
  boolean fret;
  long isecs, imicros;

  if (fallacked && qTreceive_ack != NULL)
    uwindow_acked (qdaemon, TRUE);

  /* Now we have to decide which transfer structure gets the data.  If
     ilocal is -1, it means that the protocol does not know where to
     route the data.  In that case we route it to the first transfer
     that is waiting for data, or, if none, as a new command.  If
     ilocal is 0, we either select based on the remote channel number
     or we have a new command.  */
  if (ilocal == -1 && qTreceive != NULL)
    q = qTreceive;
  else if (ilocal == 0 && iremote > 0 && aqTremote[iremote] != NULL)
    q = aqTremote[iremote];
  else if (ilocal <= 0)
    {
      const char *znull;

      ulog_user ((const char *) NULL);

      /* This data is part of a command.  If there is no null
	 character in the data, this string will be continued by the
	 next packet.  Otherwise this must be the last string in the
	 command, and we don't care about what comes after the null
	 byte.  */
      znull = (const char *) memchr (zfirst, '\0', cfirst);
      if (znull != NULL)
	fret = ftadd_cmd (qdaemon, zfirst, (size_t) (znull - zfirst),
			  iremote, TRUE);
      else
	{
	  fret = ftadd_cmd (qdaemon, zfirst, cfirst, iremote, FALSE);
	  if (fret && csecond > 0)
	    {
	      znull = (const char *) memchr (zsecond, '\0', csecond);
	      if (znull != NULL)
		fret = ftadd_cmd (qdaemon, zsecond,
				  (size_t) (znull - zsecond), iremote, TRUE);
	      else
		fret = ftadd_cmd (qdaemon, zsecond, csecond, iremote, FALSE);
	    }
	}

      if (pfexit != NULL && (qdaemon->fhangup || qTremote != NULL))
	*pfexit = TRUE;

      /* Time spent waiting for a new command is not charged to
         anybody.  */
      if (! fttime (qdaemon, &iTrecsecs, &iTrecmicros))
	fret = FALSE;

      return fret;
    }
  else
    {
      /* Get the transfer structure this data is intended for.  */
      q = qtchan (ilocal);
    }

#if DEBUG > 0
  if (q == NULL || q->precfn == NULL)
    {
      ulog (LOG_ERROR, "Protocol error: %lu bytes remote %d local %d",
	    (unsigned long) (cfirst + csecond),
	    iremote, ilocal);
      return FALSE;
    }
#endif

  ulog_user (q->s.zuser);

  fret = TRUE;

  if (q->zlog != NULL && ! q->fsendfile)
    {
      ulog (LOG_NORMAL, "%s", q->zlog);
      ubuffree (q->zlog);
      q->zlog = NULL;
    }

  if (cfirst == 0 || q->fcmd || ! q->frecfile || q != qTtiming_rec)
    {
      struct stransfer *qcharge;

      /* Either we are receiving some sort of command, or we are
         receiving data for a transfer other than the one we are
         currently timing.  It we are currently timing a transfer,
         charge any accumulated time to it.  Otherwise, if we
         currently have something to send, just forget about the
         accumulated time (when using a bidirectional protocol, it's
         very difficult to charge this time correctly).  Otherwise,
         charge it to whatever transfer receives it.  */
      if (! fttime (qdaemon, &isecs, &imicros))
	fret = FALSE;
      if (qTtiming_rec != NULL)
	qcharge = qTtiming_rec;
      else if (qTsend != NULL)
	qcharge = NULL;
      else
	qcharge = q;
      if (qcharge != NULL)
	{
	  DEBUG_MESSAGE4 (DEBUG_UUCP_PROTO,
			  "fgot_data: Charging %ld to %c %s %s",
			  ((isecs - iTrecsecs) * 1000000
			   + imicros - iTrecmicros),
			  qcharge->s.bcmd, qcharge->s.zfrom,
			  qcharge->s.zto);
	  qcharge->isecs += isecs - iTrecsecs;
	  qcharge->imicros += imicros - iTrecmicros;
	}
      iTrecsecs = isecs;
      iTrecmicros = imicros;

      /* If we received file data, start timing the new transfer.  */
      if (cfirst == 0 || q->fcmd || ! q->frecfile)
	qTtiming_rec = NULL;
      else
	qTtiming_rec = q;
    }

  /* If we're receiving a command, then accumulate it up to the null
     byte.  */
  if (q->fcmd)
    {
      const char *znull;

      znull = NULL;
      while (cfirst > 0)
	{
	  size_t cnew;
	  char *znew;

	  znull = (const char *) memchr (zfirst, '\0', cfirst);
	  if (znull != NULL)
	    cnew = znull - zfirst;
	  else
	    cnew = cfirst;
	  znew = zbufalc (q->ccmd + cnew + 1);
	  if (q->ccmd > 0)
	    memcpy (znew, q->zcmd, q->ccmd);
	  memcpy (znew + q->ccmd, zfirst, cnew);
	  znew[q->ccmd + cnew] = '\0';
	  ubuffree (q->zcmd);
	  q->zcmd = znew;
	  q->ccmd += cnew;

	  if (znull != NULL)
	    break;

	  zfirst = zsecond;
	  cfirst = csecond;
	  csecond = 0;
	}

      if (znull != NULL)
	{
	  char *zcmd;
	  size_t ccmd;

	  zcmd = q->zcmd;
	  ccmd = q->ccmd;
	  q->fcmd = FALSE;
	  q->zcmd = NULL;
	  q->ccmd = 0;
	  if (! (*q->precfn) (q, qdaemon, zcmd, ccmd + 1))
	    fret = FALSE;
	  ubuffree (zcmd);
	}

      if (pfexit != NULL
	  && (qdaemon->fhangup
	      || qdaemon->fmaster
	      || qTsend != NULL))
	*pfexit = TRUE;
    }
  else if (! q->frecfile || cfirst == 0)
    {
      /* We're either not receiving a file or the file transfer is
	 complete.  */
      q->frecfile = FALSE;
      if (! (*q->precfn) (q, qdaemon, zfirst, cfirst))
	fret = FALSE;
      if (fret && csecond > 0)
	return fgot_data (qdaemon, zsecond, csecond,
			  (const char *) NULL, (size_t) 0,
			  ilocal, iremote, ipos + (long) cfirst,
			  FALSE, pfexit);
      if (pfexit != NULL
	  && (qdaemon->fhangup
	      || qdaemon->fmaster
	      || qTsend != NULL))
	*pfexit = TRUE;
    }
  else
    {
      if (ipos != -1 && ipos != q->ipos)
	{
	  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO,
			  "fgot_data: Seeking to %ld", ipos);
	  if (! ffileseek (q->e, ipos))
	    {
	      ulog (LOG_ERROR, "seek: %s", strerror (errno));
	      fret = FALSE;
	    }
	  q->ipos = ipos;
	}

      if (fret)
	{
	  while (cfirst > 0)
	    {
	      cwrote = cfilewrite (q->e, (char *) zfirst, cfirst);
	      if (cwrote == cfirst)
		{
#if FREE_SPACE_DELTA > 0
		  long cfree_space;

		  /* Check that there is still enough space on the
		     disk.  If there isn't, we drop the connection,
		     because we have no way to abort a file transfer
		     in progress.  */
		  cfree_space = qdaemon->qsys->uuconf_cfree_space;
		  if (cfree_space > 0
		      && ((q->cbytes / FREE_SPACE_DELTA)
			  != (q->cbytes + cfirst) / FREE_SPACE_DELTA)
		      && ! frec_check_free (q, cfree_space))
		    {
		      fret = FALSE;
		      break;
		    }
#endif
		  q->cbytes += cfirst;
		  q->ipos += cfirst;
		}
	      else
		{
		  if (ffileioerror (q->e, cwrote))
		    ulog (LOG_ERROR, "write: %s", strerror (errno));
		  else
		    ulog (LOG_ERROR,
			  "Wrote %d to file when trying to write %lu",
			  cwrote, (unsigned long) cfirst);

		  /* Any write error is almost certainly a temporary
		     condition, or else UUCP would not be functioning
		     at all.  If we continue to accept the file, we
		     will wind up rejecting it at the end (what else
		     could we do?)  and the remote system will throw
		     away the request.  We're better off just dropping
		     the connection, which is what happens when we
		     return FALSE, and trying again later.  */
		  fret = FALSE;
		  break;
		}

	      zfirst = zsecond;
	      cfirst = csecond;
	      csecond = 0;
	    }
	}

      if (pfexit != NULL && qdaemon->fhangup)
	*pfexit = TRUE;
    }

  return fret;
}

/* Accumulate a string into a command.  If the command is complete,
   start up a new transfer.  */

static boolean
ftadd_cmd (qdaemon, z, clen, iremote, flast)
     struct sdaemon *qdaemon;
     const char *z;
     size_t clen;
     int iremote;
     boolean flast;
{
  static char *zbuf;
  static size_t cbuf;
  size_t cneed;
  struct scmd s;

  cneed = cTcmdlen + clen + 1;
  if (cneed > cbuf)
    {
      zbuf = (char *) xrealloc ((pointer) zbuf, cneed);
      cbuf = cneed;
    }

  memcpy (zbuf + cTcmdlen, z, clen);
  zbuf[cTcmdlen + clen] = '\0';

  if (! flast)
    {
      cTcmdlen += clen;
      return TRUE;
    }

  /* Don't save this string for next time.  */
  cTcmdlen = 0;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO,
		  "ftadd_cmd: Got command \"%s\"", zbuf);

  if (! fparse_cmd (zbuf, &s)
      || s.bcmd == 'P')
    {
      ulog (LOG_ERROR, "Received garbled command \"%s\"", zbuf);
      return TRUE;
    }

  /* Some systems seem to sometimes send garbage at the end of the
     command.  Avoid interpreting it as a size if sizes are not
     supported.  */
  if ((qdaemon->ifeatures & FEATURE_SIZES) == 0)
    s.cbytes = -1;

  if (s.bcmd != 'H' && s.bcmd != 'Y' && s.bcmd != 'N')
    ulog_user (s.zuser);
  else
    ulog_user ((const char *) NULL);

  switch (s.bcmd)
    {
    case 'S':
    case 'E':
      return fremote_send_file_init (qdaemon, &s, iremote);
    case 'R':
      return fremote_rec_file_init (qdaemon, &s, iremote);
    case 'X':
      return fremote_xcmd_init (qdaemon, &s, iremote);
    case 'H':
      /* This is a remote request for a hangup.  We close the log
	 files so that they may be moved at this point.  */
      ulog_close ();
      ustats_close ();
      {
	struct stransfer *q;

	q = qtransalc ((struct scmd *) NULL);
	q->psendfn = fremote_hangup_reply;
	q->iremote = iremote;
	q->s.bcmd = 'H';
	return fqueue_remote (qdaemon, q);
      }
    case 'N':
      /* This means a hangup request is being denied; we just ignore
	 this and wait for further commands.  */
      return TRUE;
    case 'Y':
      /* This is a remote confirmation of a hangup.  We reconfirm.  */
      if (qdaemon->fhangup)
	return TRUE;
#if DEBUG > 0
      if (qdaemon->fmaster)
	ulog (LOG_ERROR, "Got hangup reply as master");
#endif
      /* Don't check errors rigorously here, since the other side
	 might jump the gun and hang up.  The fLog_sighup variable
	 will get set TRUE again when the port is closed.  */
      fLog_sighup = FALSE;
      (void) (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, iremote);
      qdaemon->fhangup = TRUE;
      return TRUE;
#if DEBUG > 0
    default:
      ulog (LOG_FATAL, "ftadd_cmd: Can't happen");
      return FALSE;
#endif
    }
}

/* The remote system is requesting a hang up.  If we have something to
   do, send an HN.  Otherwise send two HY commands (the other side is
   presumed to send an HY command between the first and second, but we
   don't bother to wait for it) and hang up.  */

static boolean
fremote_hangup_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  boolean fret;

  utransfree (qtrans);

  if (qTremote == NULL
      && qTlocal == NULL
      && qTsend == NULL
      && qTreceive == NULL)
    {
      if (! fqueue (qdaemon, (boolean *) NULL))
	return FALSE;

      if (qTlocal == NULL)
	{
	  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: No work");
	  fret = ((*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, 0)
		  && (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, 0));
	  qdaemon->fhangup = TRUE;
	  return fret;
	}
    }

  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: Found work");
  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, "HN", 0, 0);
  qdaemon->fmaster = TRUE;
  return fret;
}

/* As described in system.h, we need to keep track of which files have
   been successfully received for which we do not know that the other
   system has received our acknowledgement.  This routine is called to
   keep a list of such files.  */

static struct sreceive_ack *qTfree_receive_ack;

void
usent_receive_ack (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  struct sreceive_ack *q;

  if (qTfree_receive_ack == NULL)
    q = (struct sreceive_ack *) xmalloc (sizeof (struct sreceive_ack));
  else
    {
      q = qTfree_receive_ack;
      qTfree_receive_ack = q->qnext;
    }

  q->qnext = qTreceive_ack;
  q->zto = zbufcpy (qtrans->s.zto);
  q->ztemp = zbufcpy (qtrans->s.ztemp);
  q->fmarked = FALSE;

  qTreceive_ack = q;
}

/* This routine is called by the protocol code when either all
   outstanding data has been acknowledged or one complete window has
   passed.  It may be called directly by the protocol, or it may be
   called via fgot_data.  If one complete window has passed, then all
   unmarked receives are marked, and we know that all marked ones have
   been acked.  */

void
uwindow_acked (qdaemon, fallacked)
     struct sdaemon *qdaemon;
     boolean fallacked;
{
  register struct sreceive_ack **pq;

  pq = &qTreceive_ack;
  while (*pq != NULL)
    {
      if (fallacked || (*pq)->fmarked)
	{
	  struct sreceive_ack *q;

	  q = *pq;
	  (void) fsysdep_forget_reception (qdaemon->qsys, q->zto,
					   q->ztemp);
	  ubuffree (q->zto);
	  ubuffree (q->ztemp);
	  *pq = q->qnext;
	  q->qnext = qTfree_receive_ack;
	  qTfree_receive_ack = q;
	}
      else
	{
	  (*pq)->fmarked = TRUE;
	  pq = &(*pq)->qnext;
	}
    }
}

/* This routine is called when an error occurred and we are crashing
   out of the connection.  It is used to report statistics on failed
   transfers to the statistics file, and it also discards useless
   temporary files for file receptions.  Note that the number of bytes
   we report as having been sent has little or nothing to do with the
   number of bytes the remote site actually received.  */

void
ufailed (qdaemon)
     struct sdaemon *qdaemon;
{
  register struct stransfer *q;

  if (qTsend != NULL)
    {
      q = qTsend;
      do
	{
	  if ((q->fsendfile || q->frecfile)
	      && q->cbytes > 0)
	    {
	      ustats (FALSE, q->s.zuser, qdaemon->qsys->uuconf_zname,
		      q->fsendfile, q->cbytes, q->isecs, q->imicros,
		      qdaemon->fcaller);
	      if (q->fsendfile)
		qdaemon->csent += q->cbytes;
	      else
		qdaemon->creceived += q->cbytes;
	    }
	  if (q->frecfile)
	    (void) frec_discard_temp (qdaemon, q);
	  q = q->qnext;
	}
      while (q != qTsend);
    }

  if (qTreceive != NULL)
    {
      q = qTreceive;
      do
	{
	  if ((q->fsendfile || q->frecfile)
	      && q->cbytes > 0)
	    {
	      ustats (FALSE, q->s.zuser, qdaemon->qsys->uuconf_zname,
		      q->fsendfile, q->cbytes, q->isecs, q->imicros,
		      qdaemon->fcaller);
	      if (q->fsendfile)
		qdaemon->csent += q->cbytes;
	      else
		qdaemon->creceived += q->cbytes;
	    }
	  if (q->frecfile)
	    (void) frec_discard_temp (qdaemon, q);
	  q = q->qnext;
	}
      while (q != qTreceive);
    }
}

/* When a local poll file is found, it is entered on the queue like
   any other job.  When it is pulled off the queue, this function is
   called.  It just calls fsysdep_did_work, which will remove the poll
   file.  This ensures that poll files are only removed if the system
   is actually called.  */

/*ARGSUSED*/
static boolean
flocal_poll_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  boolean fret;

  fret = fsysdep_did_work (qtrans->s.pseq);
  utransfree (qtrans);
  return fret;
}
