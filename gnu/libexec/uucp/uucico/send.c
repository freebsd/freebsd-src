/* send.c
   Routines to send a file.

   Copyright (C) 1991, 1992, 1993, 1994, 1995 Ian Lance Taylor

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
const char send_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucico/send.c,v 1.8 1999/08/27 23:33:50 peter Exp $";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "prot.h"
#include "trans.h"

/* We keep this information in the pinfo field of the stransfer
   structure.  */
struct ssendinfo
{
  /* Local user to send mail to (may be NULL).  */
  char *zmail;
  /* Full file name.  */
  char *zfile;
  /* Number of bytes in file.  */
  long cbytes;
  /* TRUE if this was a local request.  */
  boolean flocal;
  /* TRUE if this is a spool directory file.  */
  boolean fspool;
  /* TRUE if the file has been completely sent.  */
  boolean fsent;
  /* TRUE if the file send will never succeed; used by
     flocal_send_cancelled.  */
  boolean fnever;
  /* Execution file for sending an unsupported E request.  */
  char *zexec;
  /* Confirmation command received in fsend_await_confirm.  */
  char *zconfirm;
};

/* Local functions.  */

static void usfree_send P((struct stransfer *qtrans));
static boolean flocal_send_fail P((struct scmd *qcmd,
				   struct sdaemon *qdaemon,
				   const char *zwhy));
static boolean flocal_send_request P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon));
static boolean flocal_send_await_reply P((struct stransfer *qtrans,
					  struct sdaemon *qdaemon,
					  const char *zdata, size_t cdata));
static boolean flocal_send_cancelled P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean flocal_send_open_file P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_fail P((struct sdaemon *qdaemon,
				   enum tfailure twhy, int iremote));
static boolean fremote_rec_fail_send P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_reply P((struct stransfer *qtrans,
				    struct sdaemon *qdaemon));
static boolean fsend_file_end P((struct stransfer *qtrans,
				 struct sdaemon *qdaemon));
static boolean fsend_await_confirm P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon,
				      const char *zdata, size_t cdata));
static boolean fsend_exec_file_init P((struct stransfer *qtrans,
				       struct sdaemon *qdaemon));
static void usadd_exec_line P((char **pz, size_t *pcalc, size_t *pclen,
			       int bcmd, const char *z1, const char *z2));
static boolean fsend_exec_file P((struct stransfer *qtrans,
				  struct sdaemon *qdaemon));

/* Free up a send stransfer structure.  */

static void
usfree_send (qtrans)
     struct stransfer *qtrans;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  if (qinfo != NULL)
    {
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      ubuffree (qinfo->zexec);
      ubuffree (qinfo->zconfirm);
      xfree (qtrans->pinfo);
    }

  utransfree (qtrans);
}      

/* Set up a local request to send a file.  This may be called before
   we have even tried to call the remote system.

   If we are using a traditional protocol, which doesn't support
   channel numbers and doesn't permit the file to be sent until an
   acknowledgement has been received, the sequence of function calls
   looks like this:

   flocal_send_file_init --> fqueue_local
   flocal_send_request (sends S request) --> fqueue_receive
   flocal_send_await_reply (waits for SY) --> fqueue_send
   flocal_send_open_file (opens file, calls pffile) --> fqueue_send
   send file
   fsend_file_end (calls pffile) --> fqueue_receive
   fsend_await_confirm (waits for CY)

   If flocal_send_await_reply gets an SN, it deletes the request.  If
   the SY reply contains a file position at which to start sending,
   flocal_send_await_reply sets qinfo->ipos.

   This gets more complex if the protocol supports channels.  In that
   case, we want to start sending the file data immediately, to avoid
   the round trip delay between flocal_send_request and
   flocal_send_await_reply.  To do this, flocal_send_request calls
   fqueue_send rather than fqueue_receive.  The main execution
   sequence looks like this:

   flocal_send_file_init --> fqueue_local
   flocal_send_request (sends S request) --> fqueue_send
   flocal_send_open_file (opens file, calls pffile) --> fqueue_send
   send file
   fsend_file_end (calls pffile) --> fqueue_receive
   sometime: flocal_send_await_reply (waits for SY)
   fsend_await_confirm (waits for CY)

   In this case flocal_send_await_reply must be run before
   fsend_await_confirm; it may be run anytime after
   flocal_send_request.

   If flocal_send_await_reply is called before the entire file has
   been sent: if it gets an SN, it sets the file position to the end
   and arranges to call flocal_send_cancelled.  If it gets a file
   position request, it must adjust the file position accordingly.

   If flocal_send_await_reply is called after the entire file has been
   sent: if it gets an SN, it can simply delete the request.  It can
   ignore any file position request.

   If the request is not deleted, flocal_send_await_reply must arrange
   for the next string to be passed to fsend_await_confirm.
   Presumably fsend_await_confirm will only be called after the entire
   file has been sent.

   Just to make things even more complex, these same routines support
   sending execution requests, since that is much like sending a file.
   For an execution request, the bcmd character will be E rather than
   S.  If an execution request is being sent to a system which does
   not support them, it must be sent as two S requests instead.  The
   second one will be the execution file, but no actual file is
   created; instead the zexec and znext fields in the ssendinfo
   structure are used.  So if the bcmd character is E, then if the
   zexec field is NULL, the data file is being sent, otherwise the
   fake execution file is being sent.  */

boolean
flocal_send_file_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  const struct uuconf_system *qsys;
  boolean fspool;
  char *zfile;
  long cbytes;
  struct ssendinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (qdaemon->fcaller
      ? ! qsys->uuconf_fcall_transfer
      : ! qsys->uuconf_fcalled_transfer)
    {
      /* uux or uucp should have already made sure that the transfer
	 is possible, but it might have changed since then.  */
      if (! qsys->uuconf_fcall_transfer
	  && ! qsys->uuconf_fcalled_transfer)
	return flocal_send_fail (qcmd, qdaemon,
				 "not permitted to transfer files");

      /* We can't do the request now, but it may get done later.  */
      return TRUE;
    }

  /* The 'C' option means that the file has been copied to the spool
     directory.  */
  if (strchr (qcmd->zoptions, 'C') == NULL
      && ! fspool_file (qcmd->zfrom))
    {
      fspool = FALSE;
      if (! fin_directory_list (qcmd->zfrom,
				qsys->uuconf_pzlocal_send,
				qsys->uuconf_zpubdir, TRUE,
				TRUE, qcmd->zuser))
	return flocal_send_fail (qcmd, qdaemon, "not permitted to send");
      zfile = zbufcpy (qcmd->zfrom);
    }
  else
    {
      fspool = TRUE;
      zfile = zsysdep_spool_file_name (qsys, qcmd->ztemp, qcmd->pseq);
      if (zfile == NULL)
	return FALSE;
    }

  /* Make sure we meet any local size restrictions.  The connection
     may not have been opened at this point, so we can't check remote
     size restrictions.  */
  cbytes = csysdep_size (zfile);
  if (cbytes < 0)
    {
      ubuffree (zfile);
      if (cbytes != -1)
	return flocal_send_fail (qcmd, qdaemon, "can not get size");
      /* A cbytes value of -1 means that the file does not exist.
	 This can happen legitimately if it has already been sent from
	 the spool directory.  */
      if (! fspool)
	return flocal_send_fail (qcmd, qdaemon, "does not exist");
      (void) fsysdep_did_work (qcmd->pseq);
      return TRUE;
    }

  if (qdaemon->clocal_size != -1
      && qdaemon->clocal_size < cbytes)
    {
      ubuffree (zfile);

      if (qdaemon->cmax_ever == -2)
	{
	  long c1, c2;

	  c1 = cmax_size_ever (qsys->uuconf_qcall_local_size);
	  c2 = cmax_size_ever (qsys->uuconf_qcalled_local_size);
	  if (c1 > c2)
	    qdaemon->cmax_ever = c1;
	  else
	    qdaemon->cmax_ever = c2;
	}
		      
      if (qdaemon->cmax_ever != -1
	  && qdaemon->cmax_ever < qcmd->cbytes)
	return flocal_send_fail (qcmd, qdaemon, "too large to send");

      return TRUE;
    }

  /* We are now prepared to send the command to the remote system.  We
     queue up a transfer request to send the command when we are
     ready.  */
  qinfo = (struct ssendinfo *) xmalloc (sizeof (struct ssendinfo));
  if (strchr (qcmd->zoptions, 'm') == NULL)
    qinfo->zmail = NULL;
  else
    qinfo->zmail = zbufcpy (qcmd->zuser);
  qinfo->zfile = zfile;
  qinfo->cbytes = cbytes;
  qinfo->flocal = strchr (qcmd->zuser, '!') == NULL;
  qinfo->fspool = fspool;
  qinfo->fsent = FALSE;
  qinfo->zexec = NULL;
  qinfo->zconfirm = NULL;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = flocal_send_request;
  qtrans->pinfo = (pointer) qinfo;

  return fqueue_local (qdaemon, qtrans);
}

/* Clean up after a failing local send request.  If zwhy is not NULL,
   this reports an error to the log file and to the user.  */

static boolean
flocal_send_fail (qcmd, qdaemon, zwhy)
     struct scmd *qcmd;
     struct sdaemon *qdaemon;
     const char *zwhy;
{
  if (zwhy != NULL)
    {
      const char *zfrom;
      char *zfree;
      const char *ztemp;

      if (qcmd->bcmd != 'E')
	{
	  zfrom = qcmd->zfrom;
	  zfree = NULL;
	}
      else
	{
	  zfree = zbufalc (strlen (qcmd->zfrom)
			   + sizeof " (execution of \"\")"
			   + strlen (qcmd->zcmd));
	  sprintf (zfree, "%s (execution of \"%s\")", qcmd->zfrom,
		   qcmd->zcmd);
	  zfrom = zfree;
	}

      ulog (LOG_ERROR, "%s: %s", zfrom, zwhy);

      /* We only save the temporary file if this is a request from the
	 local system; otherwise a remote system could launch a denial
	 of service attack by filling up the .Preserve directory
	 (local users have much simpler methods for this type of
	 denial of service attack, so there is little point to using a
	 more sophisticated scheme).  */
      if (strchr (qcmd->zuser, '!') == NULL)
	ztemp = zsysdep_save_temp_file (qcmd->pseq);
      else
	ztemp = NULL;
      (void) fmail_transfer (FALSE, qcmd->zuser, (const char *) NULL,
			     zwhy, zfrom, (const char *) NULL,
			     qcmd->zto, qdaemon->qsys->uuconf_zname, ztemp);

      ubuffree (zfree);
    }

  (void) fsysdep_did_work (qcmd->pseq);

  return TRUE;
}

/* This is called when we are ready to send the request to the remote
   system.  We form the request and send it over.  If the protocol
   does not support multiple channels, we start waiting for the
   response; otherwise we can start sending the file immediately.  */

static boolean
flocal_send_request (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zsend;
  const char *znotify;
  char absize[20];
  boolean fret;

  /* Make sure the file meets any remote size restrictions.  */
  if (qdaemon->cmax_receive != -1
      && qdaemon->cmax_receive < qinfo->cbytes)
    {
      fret = flocal_send_fail (&qtrans->s, qdaemon, "too large for receiver");
      usfree_send (qtrans);
      return fret;
    }

  /* Make sure the file still exists--it may have been removed between
     the conversation startup and now.  After we have sent over the S
     command we must give an error if we can't find the file.  */
  if (! fsysdep_file_exists (qinfo->zfile))
    {
      (void) fsysdep_did_work (qtrans->s.pseq);
      usfree_send (qtrans);
      return TRUE;
    }

  /* If we are using a protocol which can make multiple channels, then
     we can open and send the file whenever we are ready.  This is
     because we will be able to distinguish the response by the
     channel it is directed to.  This assumes that every protocol
     which supports multiple channels also supports sending the file
     position in mid-stream, since otherwise we would not be able to
     restart files.  */
  qtrans->fcmd = TRUE;
  qtrans->psendfn = flocal_send_open_file;
  qtrans->precfn = flocal_send_await_reply;

  if (qdaemon->cchans > 1)
    fret = fqueue_send (qdaemon, qtrans);
  else
    fret = fqueue_receive (qdaemon, qtrans);
  if (! fret)
    return FALSE;

  /* Construct the notify string to send.  If we are going to send a
     size or an execution command, it must be non-empty.  */
  znotify = qtrans->s.znotify;
  if (znotify == NULL)
    znotify = "";
  if ((qdaemon->ifeatures & FEATURE_SIZES) != 0
      || (qtrans->s.bcmd == 'E'
	  && (qdaemon->ifeatures & FEATURE_EXEC) != 0))
    {
      if (*znotify == '\0')
	znotify = "\"\"";
    }
  else
    {
      /* We don't need a notify string.  Some crufty UUCP code can't
	 handle a pair of double quotes.  */
      if (strcmp (znotify, "\"\"") == 0)
	znotify = "";
    }

  /* Construct the size string to send.  */
  if ((qdaemon->ifeatures & FEATURE_SIZES) == 0
      && (qtrans->s.bcmd != 'E'
	  || (qdaemon->ifeatures & FEATURE_EXEC) == 0))
    absize[0] = '\0';
  else if ((qdaemon->ifeatures & FEATURE_V103) == 0)
    sprintf (absize, "0x%lx", (unsigned long) qinfo->cbytes);
  else
    sprintf (absize, "%ld", qinfo->cbytes);

  zsend = zbufalc (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
		   + strlen (qtrans->s.zuser) + strlen (qtrans->s.zoptions)
		   + strlen (qtrans->s.ztemp) + strlen (znotify)
		   + strlen (absize)
		   + (qtrans->s.zcmd != NULL ? strlen (qtrans->s.zcmd) : 0)
		   + 50);

  /* If this an execution request and the other side supports
     execution requests, we send an E command.  Otherwise we send an S
     command.  The case of an execution request when we are sending
     the fake execution file is handled just like an S request at this
     point.  */
  if (qtrans->s.bcmd == 'E'
      && (qdaemon->ifeatures & FEATURE_EXEC) != 0)
    {
      /* Send the string
	 E zfrom zto zuser zoptions ztemp imode znotify size zcmd
	 to the remote system.  We put a '-' in front of the (possibly
	 empty) options and a '0' in front of the mode.  */
      sprintf (zsend, "E %s %s %s -%s %s 0%o %s %s %s", qtrans->s.zfrom,
	       qtrans->s.zto, qtrans->s.zuser, qtrans->s.zoptions,
	       qtrans->s.ztemp, qtrans->s.imode, znotify, absize,
	       qtrans->s.zcmd);
    }
  else
    {
      const char *zoptions, *zdummy;

      /* Send the string
	 S zfrom zto zuser zoptions ztemp imode znotify
	 to the remote system.  We put a '-' in front of the (possibly
	 empty) options and a '0' in front of the mode.  If size
	 negotiation is supported, we also send the size; in this case
	 if znotify is empty we must send it as "".  If this is really
	 an execution request, we have to simplify the options string
	 to remove the various execution options which may confuse the
	 remote system.  SVR4 expects a string "dummy" between the
	 notify string and the size; I don't know why.  */
      if (qtrans->s.bcmd != 'E')
	zoptions = qtrans->s.zoptions;
      else if (strchr (qtrans->s.zoptions, 'C') != NULL)
	{
	  /* This should set zoptions to "C", but at least one UUCP
	     program gets confused by it.  That means that it will
	     fail in certain cases, but I suppose we might as well
	     kowtow to compatibility.  This shouldn't matter to any
	     other program, I hope.  */
	  zoptions = "";
	}
      else
	zoptions = "c";

      if ((qdaemon->ifeatures & FEATURE_SVR4) != 0)
	zdummy = " dummy ";
      else
	zdummy = " ";

      sprintf (zsend, "S %s %s %s -%s %s 0%o %s%s%s", qtrans->s.zfrom,
	       qtrans->s.zto, qtrans->s.zuser, zoptions,
	       qtrans->s.ztemp, qtrans->s.imode, znotify, zdummy,
	       absize);
    }

  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, zsend, qtrans->ilocal,
					qtrans->iremote);
  ubuffree (zsend);

  /* If fret is FALSE, we should free qtrans here, but see the comment
     at the end of flocal_rec_send_request.  */

  return fret;
}

/* This is called when a reply is received for the send request.  As
   described at length above, if the protocol supports multiple
   channels we may be in the middle of sending the file, or we may
   even finished sending the file.  */

static boolean
flocal_send_await_reply (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char bcmd;

  if (qtrans->s.bcmd == 'E'
      && (qdaemon->ifeatures & FEATURE_EXEC) != 0)
    bcmd = 'E';
  else
    bcmd = 'S';
  if (zdata[0] != bcmd
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      ulog (LOG_ERROR, "%s: Bad response to %c request: \"%s\"",
	    qtrans->s.zfrom, bcmd, zdata);
      usfree_send (qtrans);
      return FALSE;
    }

  if (zdata[1] == 'N')
    {
      const char *zerr;
      boolean fnever;

      fnever = TRUE;
      if (zdata[2] == '2')
	zerr = "permission denied by remote";
      else if (zdata[2] == '4')
	{
	  zerr = "remote cannot create work files";
	  fnever = FALSE;
	}
      else if (zdata[2] == '6')
	{
	  zerr = "too large for remote now";
	  fnever = FALSE;
	}
      else if (zdata[2] == '7')
	{
	  /* The file is too large to ever send.  */
	  zerr = "too large for remote";
	}
      else if (zdata[2] == '8')
	{
	  /* The file was already received by the remote system.  This
	     is not an error, it just means that the ack from the
	     remote was lost in the previous conversation, and there
	     is no need to resend the file.  */
	  zerr = NULL;
	}
      else if (zdata[2] == '9')
	{
	  /* Remote has run out of channels.  */
	  zerr = "too many channels for remote";
	  fnever = FALSE;

	  /* Drop one channel; using exactly one channel causes
	     slightly different behahaviour in a few places, so don't
	     decrement to one.  */
	  if (qdaemon->cchans > 2)
	    --qdaemon->cchans;
	}
      else
	zerr = "unknown reason";

      if (! fnever
	  || (qtrans->s.bcmd == 'E'
	      && (qdaemon->ifeatures & FEATURE_EXEC) == 0
	      && qinfo->zexec == NULL))
	{
	  if (qtrans->s.bcmd == 'E')
	    ulog (LOG_ERROR, "%s (execution of \"%s\"): %s",
		  qtrans->s.zfrom, qtrans->s.zcmd, zerr);
	  else
	    ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);
	}
      else
	{
	  if (! flocal_send_fail (&qtrans->s, qdaemon, zerr))
	    return FALSE;
	}

      /* If the protocol does not support multiple channels, we can
	 simply remove the transaction.  Otherwise we must make sure
	 the remote side knows that we have finished sending the file
	 data.  If we have already sent the entire file, there will be
	 no confusion.  */
      if (qdaemon->cchans == 1 || qinfo->fsent)
	{
	  /* If we are breaking a 'E' command into two 'S' commands,
	     and that was for the first 'S' command, we still have to
	     send the second one.  */
	  if (fnever
	      && qtrans->s.bcmd == 'E'
	      && (qdaemon->ifeatures & FEATURE_EXEC) == 0
	      && qinfo->zexec == NULL)
	    return fsend_exec_file_init (qtrans, qdaemon);

	  usfree_send (qtrans);
	  return TRUE;
	}
      else
	{
	  /* Seek to the end of the file so that the next read will
	     send end of file.  We have to be careful here, because we
	     may have actually already sent end of file--we could be
	     being called because of data received while the end of
	     file block was sent.  */
	  if (! ffileseekend (qtrans->e))
	    {
	      ulog (LOG_ERROR, "seek to end: %s", strerror (errno));
	      usfree_send (qtrans);
	      return FALSE;
	    }
	  qtrans->psendfn = flocal_send_cancelled;
	  qtrans->precfn = NULL;

	  qinfo->fnever = fnever;

	  return fqueue_send (qdaemon, qtrans);
	}
    }

  /* A number following the SY or EY is the file position to start
     sending from.  If we are already sending the file, we must set
     the position accordingly.  */
  if (zdata[2] != '\0')
    {
      long cskip;

      cskip = strtol ((char *) (zdata + 2), (char **) NULL, 0);
      if (cskip > 0 && qtrans->ipos < cskip)
	{
	  if (qtrans->fsendfile && ! qinfo->fsent)
	    {
	      if (! ffileseek (qtrans->e, cskip))
		{
		  ulog (LOG_ERROR, "seek: %s", strerror (errno));
		  usfree_send (qtrans);
		  return FALSE;
		}
	    }
	  qtrans->ipos = cskip;
	}
    }

  /* Now queue up to send the file or to wait for the confirmation.
     We already set psendfn at the end of flocal_send_request.  If the
     protocol supports multiple channels, we have already called
     fqueue_send; calling it again would move the request in the
     queue, which would make the log file a bit confusing.  */
  qtrans->fcmd = TRUE;
  qtrans->precfn = fsend_await_confirm;
  if (qinfo->fsent)
    return fqueue_receive (qdaemon, qtrans);
  else if (qdaemon->cchans <= 1)
    return fqueue_send (qdaemon, qtrans);
  else
    return TRUE;
}

/* Open the file, if any, and prepare to send it.  */

static boolean
flocal_send_open_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  const char *zuser;

  /* If this is not a fake execution file, open it.  */
  if (qinfo->zexec == NULL)
    {
      /* If there is an ! in the user name, this is a remote request
	 queued up by fremote_xcmd_init.  */
      zuser = qtrans->s.zuser;
      if (strchr (zuser, '!') != NULL)
	zuser = NULL;

      qtrans->e = esysdep_open_send (qdaemon->qsys, qinfo->zfile,
				     ! qinfo->fspool, zuser);
      if (! ffileisopen (qtrans->e))
	{
	  (void) fmail_transfer (FALSE, qtrans->s.zuser,
				 (const char *) NULL,
				 "cannot open file",
				 qtrans->s.zfrom, (const char *) NULL,
				 qtrans->s.zto,
				 qdaemon->qsys->uuconf_zname,
				 (qinfo->flocal
				  ? zsysdep_save_temp_file (qtrans->s.pseq)
				  : (const char *) NULL));
	  (void) fsysdep_did_work (qtrans->s.pseq);
	  usfree_send (qtrans);

	  /* Unfortunately, there is no way to cancel a file send
	     after we've already put it in progress.  So we have to
	     return FALSE to drop the connection.  */
	  return FALSE;
	}
    }

  /* If flocal_send_await_reply has received a reply with a file
     position, it will have set qtrans->ipos to the position at which
     to start.  */
  if (qtrans->ipos > 0)
    {
      if (qinfo->zexec != NULL)
	{
	  if (qtrans->ipos > qtrans->cbytes)
	    qtrans->ipos = qtrans->cbytes;
	}
      else
	{
	  if (! ffileseek (qtrans->e, qtrans->ipos))
	    {
	      ulog (LOG_ERROR, "seek: %s", strerror (errno));
	      usfree_send (qtrans);
	      return FALSE;
	    }
	}
    }

  /* We don't bother to log sending the execution file.  */
  if (qinfo->zexec == NULL)
    {
      const char *zsend;
      char *zalc;

      if (qtrans->s.bcmd != 'E')
	{
	  zsend = qtrans->s.zfrom;
	  zalc = NULL;
	}
      else
	{
	  zalc = zbufalc (strlen (qtrans->s.zcmd) + sizeof " ()"
			  + strlen (qtrans->s.zfrom));
	  sprintf (zalc, "%s (%s)", qtrans->s.zcmd, qtrans->s.zfrom);
	  zsend = zalc;
	}

      qtrans->zlog = zbufalc (sizeof "Sending ( bytes resume at )"
			      + strlen (zsend) + 50);
      sprintf (qtrans->zlog, "Sending %s (%ld bytes", zsend, qinfo->cbytes);
      if (qtrans->ipos > 0)
	sprintf (qtrans->zlog + strlen (qtrans->zlog), " resume at %ld",
		 qtrans->ipos);
      strcat (qtrans->zlog, ")");

      ubuffree (zalc);
    }

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes - qtrans->ipos,
					&fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  if (qinfo->zexec != NULL)
    qtrans->psendfn = fsend_exec_file;
  else
    {
      qtrans->fsendfile = TRUE;
      qtrans->psendfn = fsend_file_end;
    }

  return fqueue_send (qdaemon, qtrans);
}

/* Cancel a file send.  This is only called for a protocol which
   supports multiple channels.  It is needed so that both systems
   agree as to when a channel is no longer needed.  */

static boolean
flocal_send_cancelled (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  /* If we are breaking a 'E' command into two 'S' commands, and that
     was for the first 'S' command, and the first 'S' command will
     never be sent, we still have to send the second one.  */
  if (qinfo->fnever
      && qtrans->s.bcmd == 'E'
      && (qdaemon->ifeatures & FEATURE_EXEC) == 0
      && qinfo->zexec == NULL)
    return fsend_exec_file_init (qtrans, qdaemon);

  usfree_send (qtrans);
  return TRUE;
}

/* A remote request to receive a file (meaning that we have to send a
   file).  The sequence of functions calls is as follows:

   fremote_rec_file_init (open file) --> fqueue_remote
   fremote_rec_reply (send RY, call pffile) --> fqueue_send
   send file
   fsend_file_end (calls pffile) --> fqueue_receive
   fsend_await_confirm (waits for CY)
   */

boolean
fremote_rec_file_init (qdaemon, qcmd, iremote)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
     int iremote;
{
  const struct uuconf_system *qsys;
  char *zfile;
  boolean fbadname;
  long cbytes;
  unsigned int imode;
  openfile_t e;
  struct ssendinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (! qsys->uuconf_fsend_request)
    {
      ulog (LOG_ERROR, "%s: not permitted to send files to remote",
	    qcmd->zfrom);
      return fremote_rec_fail (qdaemon, FAILURE_PERM, iremote);
    }

  if (fspool_file (qcmd->zfrom))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", qcmd->zfrom);
      return fremote_rec_fail (qdaemon, FAILURE_PERM, iremote);
    }

  zfile = zsysdep_local_file (qcmd->zfrom, qsys->uuconf_zpubdir, &fbadname);
  if (zfile == NULL && fbadname)
    {
      ulog (LOG_ERROR, "%s: bad local file name", qcmd->zfrom);
      return fremote_rec_fail (qdaemon, FAILURE_PERM, iremote);
    }
  if (zfile != NULL)
    {
      char *zbased;

      zbased = zsysdep_add_base (zfile, qcmd->zto);
      ubuffree (zfile);
      zfile = zbased;
    }
  if (zfile == NULL)
    return fremote_rec_fail (qdaemon, FAILURE_PERM, iremote);

  if (! fin_directory_list (zfile, qsys->uuconf_pzremote_send,
			    qsys->uuconf_zpubdir, TRUE, TRUE,
			    (const char *) NULL))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", zfile);
      ubuffree (zfile);
      return fremote_rec_fail (qdaemon, FAILURE_PERM, iremote);
    }

  /* If the file is larger than the amount of space the other side
     reported, we can't send it.  Should we adjust this check based on
     the restart position?  */
  cbytes = csysdep_size (zfile);
  if (cbytes != -1
      && ((qcmd->cbytes != -1 && qcmd->cbytes < cbytes)
	  || (qdaemon->cremote_size != -1
	      && qdaemon->cremote_size < cbytes)
	  || (qdaemon->cmax_receive != -1
	      && qdaemon->cmax_receive < cbytes)))
    {
      ulog (LOG_ERROR, "%s: too large to send", zfile);
      ubuffree (zfile);
      return fremote_rec_fail (qdaemon, FAILURE_SIZE, iremote);
    }

  imode = ixsysdep_file_mode (zfile);

  e = esysdep_open_send (qsys, zfile, TRUE, (const char *) NULL);
  if (! ffileisopen (e))
    {
      ubuffree (zfile);
      return fremote_rec_fail (qdaemon, FAILURE_OPEN, iremote);
    }

  /* If the remote requested that the file send start from a
     particular position, arrange to do so.  */
  if (qcmd->ipos > 0)
    {
      if (! ffileseek (e, qcmd->ipos))
	{
	  ulog (LOG_ERROR, "seek: %s", strerror (errno));
	  ubuffree (zfile);
	  return FALSE;
	}
    }

  qinfo = (struct ssendinfo *) xmalloc (sizeof (struct ssendinfo));
  qinfo->zmail = NULL;
  qinfo->zfile = zfile;
  qinfo->cbytes = cbytes;
  qinfo->flocal = FALSE;
  qinfo->fspool = FALSE;
  qinfo->fsent = FALSE;
  qinfo->zexec = NULL;
  qinfo->zconfirm = NULL;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = fremote_rec_reply;
  qtrans->iremote = iremote;
  qtrans->pinfo = (pointer) qinfo;
  qtrans->e = e;
  qtrans->ipos = qcmd->ipos;
  qtrans->s.imode = imode;

  return fqueue_remote (qdaemon, qtrans);
}

/* Reply to a receive request from the remote system, and prepare to
   start sending the file.  */

static boolean
fremote_rec_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char absend[50];

  qtrans->fsendfile = TRUE;
  qtrans->psendfn = fsend_file_end;
  qtrans->fcmd = TRUE;
  qtrans->precfn = fsend_await_confirm;

  if (! fqueue_send (qdaemon, qtrans))
    return FALSE;

  qtrans->zlog = zbufalc (sizeof "Sending ( bytes) "
			  + strlen (qtrans->s.zfrom) + 25);
  sprintf (qtrans->zlog, "Sending %s (%ld bytes)", qtrans->s.zfrom,
	   qinfo->cbytes);

  /* We send the file size because SVR4 UUCP does.  We don't look for
     it.  We send a trailing M if we want to request a hangup.  We
     send it both after the mode and at the end of the entire string;
     I don't know where programs look for it.  */
  if (qdaemon->frequest_hangup)
    DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO,
		    "fremote_rec_reply: Requesting remote to transfer control");
  sprintf (absend, "RY 0%o%s 0x%lx%s", qtrans->s.imode,
	   qdaemon->frequest_hangup ? "M" : "",
	   (unsigned long) qinfo->cbytes,
	   qdaemon->frequest_hangup ? "M" : "");
  if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, absend, qtrans->ilocal,
				       qtrans->iremote))
    {
      (void) ffileclose (qtrans->e);
      /* Should probably free qtrans here, but see the comment at the
         end of flocal_rec_send_request.  */
      return FALSE;
    }

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes, &fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}
    }

  return TRUE;
}

/* If we can't send a file as requested by the remote system, queue up
   a failure reply which will be sent when possible.  */

static boolean
fremote_rec_fail (qdaemon, twhy, iremote)
     struct sdaemon *qdaemon;
     enum tfailure twhy;
     int iremote;
{
  enum tfailure *ptinfo;
  struct stransfer *qtrans;

  ptinfo = (enum tfailure *) xmalloc (sizeof (enum tfailure));
  *ptinfo = twhy;

  qtrans = qtransalc ((struct scmd *) NULL);
  qtrans->psendfn = fremote_rec_fail_send;
  qtrans->iremote = iremote;
  qtrans->pinfo = (pointer) ptinfo;

  return fqueue_remote (qdaemon, qtrans);
}

/* Send a failure string for a receive command to the remote system;
   this is called when we are ready to reply to the command.  */

static boolean
fremote_rec_fail_send (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  enum tfailure *ptinfo = (enum tfailure *) qtrans->pinfo;
  const char *z;
  int ilocal, iremote;

  switch (*ptinfo)
    {
    case FAILURE_PERM:
    case FAILURE_OPEN:
      z = "RN2";
      break;
    case FAILURE_SIZE:
      z = "RN6";
      break;
    default:
      z = "RN";
      break;
    }
  
  ilocal = qtrans->ilocal;
  iremote = qtrans->iremote;

  xfree (qtrans->pinfo);
  utransfree (qtrans);

  return (*qdaemon->qproto->pfsendcmd) (qdaemon, z, ilocal, iremote);
}

/* This is called when the main loop has finished sending a file.  It
   prepares to wait for a response from the remote system.  Note that
   if this is a local request and the protocol supports multiple
   channels, we may not even have received a confirmation of the send
   request.  */

static boolean
fsend_file_end (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, FALSE, TRUE,
					(long) -1, &fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qinfo->fsent = TRUE;

  /* If zconfirm is set, then we have already received the
     confirmation, and should call fsend_await_confirm directly.  */
  if (qinfo->zconfirm != NULL)
    return fsend_await_confirm (qtrans, qdaemon, qinfo->zconfirm,
				strlen (qinfo->zconfirm) + 1);

  /* qtrans->precfn should have been set by a previous function.  */
  return fqueue_receive (qdaemon, qtrans);
}

/* Handle the confirmation string received after sending a file.  */

/*ARGSUSED*/
static boolean
fsend_await_confirm (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  boolean fnever;
  const char *zerr;

  /* If fsent is FALSE, it means that we have received the
     confirmation before fsend_file_end got called.  To avoid
     confusion, we save away the confirmation message, and let
     fsend_file_end call us directly.  If we did not do this, we would
     have to fix a thorny race condition in floop, which wants to
     refer to the qtrans structure after sending the end of the file.  */
  if (! qinfo->fsent)
    {
      qinfo->zconfirm = zbufcpy (zdata);
      return TRUE;
    }

  if (qinfo->zexec == NULL)
    (void) ffileclose (qtrans->e);

  fnever = FALSE;
  if (zdata[0] != 'C'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      zerr = "bad confirmation from remote";
      ulog (LOG_ERROR, "%s: %s \"%s\"", qtrans->s.zfrom, zerr, zdata);
    }
  else if (zdata[1] == 'N')
    {
      fnever = TRUE;
      if (zdata[2] == '5')
	{
	  zerr = "file could not be stored in final location";
	  ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);
	}
      else
	{
	  zerr = "file send failed for unknown reason";
	  ulog (LOG_ERROR, "%s: %s \"%s\"", qtrans->s.zfrom, zerr, zdata);
	}
    }
  else
    {
      zerr = NULL;

      /* If we receive CYM, it means that the other side wants us to
	 hang up so that they can send us something.  The
	 fhangup_requested field is checked in the main loop.  */
      if (zdata[2] == 'M' && qdaemon->fmaster)
	{
	  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO,
			  "fsend_await_confirm: Remote has requested transfer of control");
	  qdaemon->fhangup_requested = TRUE;
	}
    }

  ustats (zerr == NULL, qtrans->s.zuser, qdaemon->qsys->uuconf_zname,
	  TRUE, qtrans->cbytes, qtrans->isecs, qtrans->imicros,
	  qdaemon->fcaller);
  qdaemon->csent += qtrans->cbytes;

  if (zerr == NULL)
    {
      /* If this is an execution request, and the remote system
	 doesn't support execution requests, we have to set up the
	 fake execution file and loop around again.  */
      if (qtrans->s.bcmd == 'E'
	  && (qdaemon->ifeatures & FEATURE_EXEC) == 0
	  && qinfo->zexec == NULL)
	return fsend_exec_file_init (qtrans, qdaemon);

      /* Send mail about the transfer if requested.  */
      if (qinfo->zmail != NULL && *qinfo->zmail != '\0')
	(void) fmail_transfer (TRUE, qtrans->s.zuser, qinfo->zmail,
			       (const char *) NULL,
			       qtrans->s.zfrom, (const char *) NULL,
			       qtrans->s.zto, qdaemon->qsys->uuconf_zname,
			       (const char *) NULL);

      if (qtrans->s.pseq != NULL)
	(void) fsysdep_did_work (qtrans->s.pseq);
    }
  else
    {
      /* If the file send failed, we only try to save the file and
	 send mail if it was requested locally and it will never
	 succeed.  We send mail to qinfo->zmail if set, otherwise to
	 qtrans->s.zuser.  I hope this is reasonable.  */
      if (fnever && qinfo->flocal)
	{
	  (void) fmail_transfer (FALSE, qtrans->s.zuser, qinfo->zmail,
				 zerr, qtrans->s.zfrom, (const char *) NULL,
				 qtrans->s.zto, qdaemon->qsys->uuconf_zname,
				 zsysdep_save_temp_file (qtrans->s.pseq));
	  (void) fsysdep_did_work (qtrans->s.pseq);
	}
    }

  usfree_send (qtrans);

  return TRUE;
}

/* Prepare to send an execution file to a system which does not
   support execution requests.  We build the execution file in memory,
   and then call flocal_send_request as though we were sending a real
   file.  Instead of sending a file, the code in flocal_send_open_file
   will arrange to call fsend_exec_file which will send data out of
   the buffer we have created.  */

static boolean
fsend_exec_file_init (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zxqtfile;
  char abtname[CFILE_NAME_LEN];
  char abxname[CFILE_NAME_LEN];
  char *z;
  size_t calc, clen;

  z = NULL;
  calc = 0;
  clen = 0;

  usadd_exec_line (&z, &calc, &clen, 'U', qtrans->s.zuser,
		   qdaemon->zlocalname);
  usadd_exec_line (&z, &calc, &clen, 'F', qtrans->s.zto, "");
  usadd_exec_line (&z, &calc, &clen, 'I', qtrans->s.zto, "");
  if (strchr (qtrans->s.zoptions, 'N') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'N', "", "");
  if (strchr (qtrans->s.zoptions, 'Z') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'Z', "", "");
  if (strchr (qtrans->s.zoptions, 'R') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'R', qtrans->s.znotify, "");
  if (strchr (qtrans->s.zoptions, 'e') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'e', "", "");
  usadd_exec_line (&z, &calc, &clen, 'C', qtrans->s.zcmd, "");

  qinfo->zexec = z;
  qinfo->cbytes = clen;

  zxqtfile = zsysdep_data_file_name (qdaemon->qsys, qdaemon->zlocalname,
				     BDEFAULT_UUX_GRADE, TRUE, abtname,
				     (char *) NULL, abxname);
  if (zxqtfile == NULL)
    {
      usfree_send (qtrans);
      return FALSE;
    }
  ubuffree (zxqtfile);

  ubuffree ((char *) qtrans->s.zfrom);
  qtrans->s.zfrom = zbufcpy (abtname);
  ubuffree ((char *) qtrans->s.zto);
  qtrans->s.zto = zbufcpy (abxname);
  ubuffree ((char *) qtrans->s.zoptions);
  qtrans->s.zoptions = zbufcpy ("C");
  ubuffree ((char *) qtrans->s.ztemp);
  qtrans->s.ztemp = zbufcpy (abtname);

  qtrans->psendfn = flocal_send_request;
  qtrans->precfn = NULL;
  qtrans->ipos = 0;
  qtrans->cbytes = 0;
  qtrans->isecs = 0;
  qtrans->imicros = 0;
  qinfo->fsent = FALSE;
  ubuffree (qinfo->zconfirm);
  qinfo->zconfirm = NULL;

  return fqueue_send (qdaemon, qtrans);
}

/* Add a line to the fake execution file.  */

static void
usadd_exec_line (pz, pcalc, pclen, bcmd, z1, z2)
     char **pz;
     size_t *pcalc;
     size_t *pclen;
     int bcmd;
     const char *z1;
     const char *z2;
{
  size_t c1, c2;
  char *znew;

  c1 = strlen (z1);
  c2 = strlen (z2);

  if (*pclen + c1 + c2 + 4 >= *pcalc)
    {
      *pcalc += c1 + c2 + 100;
      znew = zbufalc (*pcalc);
      if (*pclen > 0)
	{
	  memcpy (znew, *pz, *pclen);
	  ubuffree (*pz);
	}
      *pz = znew;
    }

  znew = *pz + *pclen;
  *znew++ = bcmd;
  if (*z1 != '\0')
    {
      *znew++ = ' ';
      memcpy (znew, z1, c1);
      znew += c1;
      if (*z2 != '\0')
	{
	  *znew++ = ' ';
	  memcpy (znew, z2, c2);
	  znew += c2;
	}
    }

  /* In some bizarre non-Unix case we might have to worry about the
     newline here.  We don't know how a newline is normally written
     out to a file, but whatever is written to a file is what we will
     normally transfer.  If that is not simply \n then this fake
     execution file will not look like other execution files.  */
  *znew++ = '\n';

  *pclen = znew - *pz;
}

/* This routine is called to send the contents of the fake execution
   file.  Normally file data is sent by the floop routine in trans.c,
   but since we don't have an actual file we must do it here.  This
   routine sends the complete buffer, followed by a zero length
   packet, and then calls fsend_file_end.  */

static boolean
fsend_exec_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zdata;
  size_t cdata;
  size_t csend;

  zdata = (*qdaemon->qproto->pzgetspace) (qdaemon, &cdata);
  if (zdata == NULL)
    {
      usfree_send (qtrans);
      return FALSE;
    }

  csend = qinfo->cbytes - qtrans->ipos;
  if (csend > cdata)
    csend = cdata;

  memcpy (zdata, qinfo->zexec + qtrans->ipos, csend);

  if (! (*qdaemon->qproto->pfsenddata) (qdaemon, zdata, csend,
					qtrans->ilocal, qtrans->iremote,
					qtrans->ipos))
    {
      usfree_send (qtrans);
      return FALSE;
    }

  qtrans->cbytes += csend;
  qtrans->ipos += csend;

  if (csend == 0)
    return fsend_file_end (qtrans, qdaemon);

  /* Leave the job on the send queue.  */

  return TRUE;
}
