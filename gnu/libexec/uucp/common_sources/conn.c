/* conn.c
   Connection routines for the Taylor UUCP package.

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
const char conn_rcsid[] = "$Id: conn.c,v 1.1 1993/08/05 18:22:35 conklin Exp $";
#endif

#include <ctype.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"

static boolean fcdo_dial P((struct sconnection *qconn, pointer puuconf,
			    struct uuconf_dialer *qdialer,
			    const char *zphone, boolean ftranslate));

/* Create a new connection.  This relies on system dependent functions
   to set the qcmds and psysdep fields.  If qport is NULL, it opens a
   standard input port.  */

boolean
fconn_init (qport, qconn)
     struct uuconf_port *qport;
     struct sconnection *qconn;
{
  qconn->qport = qport;
  switch (qport == NULL ? UUCONF_PORTTYPE_STDIN : qport->uuconf_ttype)
    {
    case UUCONF_PORTTYPE_STDIN:
      return fsysdep_stdin_init (qconn);
    case UUCONF_PORTTYPE_MODEM:
      return fsysdep_modem_init (qconn);
    case UUCONF_PORTTYPE_DIRECT:
      return fsysdep_direct_init (qconn);
#if HAVE_TCP
    case UUCONF_PORTTYPE_TCP:
      return fsysdep_tcp_init (qconn);
#endif
#if HAVE_TLI
    case UUCONF_PORTTYPE_TLI:
      return fsysdep_tli_init (qconn);
#endif
    default:
      ulog (LOG_ERROR, "Unknown port type");
      return FALSE;
    }
}

/* Connection dispatch routines.  */

/* Free a connection.  */

void
uconn_free (qconn)
     struct sconnection *qconn;
{
  (*qconn->qcmds->pufree) (qconn);
}

/* Lock a connection.   */

boolean
fconn_lock (qconn, fin)
     struct sconnection *qconn;
     boolean fin;
{
  boolean (*pflock) P((struct sconnection *, boolean));

  pflock = qconn->qcmds->pflock;
  if (pflock == NULL)
    return TRUE;
  return (*pflock) (qconn, fin);
}

/* Unlock a connection.  */

boolean
fconn_unlock (qconn)
     struct sconnection *qconn;
{
  boolean (*pfunlock) P((struct sconnection *));

  pfunlock = qconn->qcmds->pfunlock;
  if (pfunlock == NULL)
    return TRUE;
  return (*pfunlock) (qconn);
}

/* Open a connection.  */

boolean
fconn_open (qconn, ibaud, ihighbaud, fwait)
     struct sconnection *qconn;
     long ibaud;
     long ihighbaud;
     boolean fwait;
{
  boolean fret;

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_PORT))
    {
      char abspeed[20];

      if (ibaud == (long) 0)
	strcpy (abspeed, "default speed");
      else
	sprintf (abspeed, "speed %ld", ibaud);

      if (qconn->qport == NULL)
	ulog (LOG_DEBUG, "fconn_open: Opening stdin port (%s)",
	      abspeed);
      else if (qconn->qport->uuconf_zname == NULL)
	ulog (LOG_DEBUG, "fconn_open: Opening unnamed port (%s)",
	      abspeed);
      else
	ulog (LOG_DEBUG, "fconn_open: Opening port %s (%s)",
	      qconn->qport->uuconf_zname, abspeed);
    }
#endif

  /* If the system provides a range of baud rates, we select the
     highest baud rate supported by the port.  */
  if (ihighbaud != 0 && qconn->qport != NULL)
    {
      struct uuconf_port *qport;

      qport = qconn->qport;
      ibaud = ihighbaud;
      if (qport->uuconf_ttype == UUCONF_PORTTYPE_MODEM)
	{
	  if (qport->uuconf_u.uuconf_smodem.uuconf_ilowbaud != 0)
	    {
	      if (qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud < ibaud)
		ibaud = qport->uuconf_u.uuconf_smodem.uuconf_ihighbaud;
	    }
	  else if (qport->uuconf_u.uuconf_smodem.uuconf_ibaud != 0)
	    ibaud = qport->uuconf_u.uuconf_smodem.uuconf_ibaud;
	}
      else if (qport->uuconf_ttype == UUCONF_PORTTYPE_DIRECT)
	{
	  if (qport->uuconf_u.uuconf_sdirect.uuconf_ibaud != 0)
	    ibaud = qport->uuconf_u.uuconf_sdirect.uuconf_ibaud;
	}
    }

  /* This will normally be overridden by the port specific open
     routine.  */
  if (qconn->qport == NULL)
    ulog_device ("stdin");
  else
    ulog_device (qconn->qport->uuconf_zname);

  fret = (*qconn->qcmds->pfopen) (qconn, ibaud, fwait);

  if (! fret)
    ulog_device ((const char *) NULL);

  return fret;
}

/* Close a connection.  */

boolean
fconn_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdialer;
     boolean fsuccess;
{
  boolean fret;

  DEBUG_MESSAGE0 (DEBUG_PORT, "fconn_close: Closing connection");

  /* Don't report hangup signals while we're closing.  */
  fLog_sighup = FALSE;

  fret = (*qconn->qcmds->pfclose) (qconn, puuconf, qdialer, fsuccess);

  /* Make sure any signal reporting has been done before we set
     fLog_sighup back to TRUE.  */
  ulog (LOG_ERROR, (const char *) NULL);
  fLog_sighup = TRUE;

  ulog_device ((const char *) NULL);

  return fret;
}

/* Reset the connection.  */

boolean
fconn_reset (qconn)
     struct sconnection *qconn;
{
  DEBUG_MESSAGE0 (DEBUG_PORT, "fconn_reset: Resetting connection");

  return (*qconn->qcmds->pfreset) (qconn);
}

/* Dial out on the connection.  */

boolean
fconn_dial (qconn, puuconf, qsys, zphone, qdialer, ptdialerfound)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_system *qsys;
     const char *zphone;
     struct uuconf_dialer *qdialer;
     enum tdialerfound *ptdialerfound;
{
  struct uuconf_dialer sdialer;
  enum tdialerfound tfound;
  boolean (*pfdial) P((struct sconnection *, pointer,
		       const struct uuconf_system *, const char *,
		       struct uuconf_dialer *, enum tdialerfound *));

  if (qdialer == NULL)
    qdialer = &sdialer;
  if (ptdialerfound == NULL)
    ptdialerfound = &tfound;
      
  qdialer->uuconf_zname = NULL;
  *ptdialerfound = DIALERFOUND_FALSE;

  pfdial = qconn->qcmds->pfdial;
  if (pfdial == NULL)
    return TRUE;
  return (*pfdial) (qconn, puuconf, qsys, zphone, qdialer, ptdialerfound);
}

/* Read data from the connection.  */

boolean
fconn_read (qconn, zbuf, pclen, cmin, ctimeout, freport)
     struct sconnection *qconn;
     char *zbuf;
     size_t *pclen;
     size_t cmin;
     int ctimeout;
     boolean freport;
{
  boolean fret;

  fret = (*qconn->qcmds->pfread) (qconn, zbuf, pclen, cmin, ctimeout,
				  freport);

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_INCOMING))
    udebug_buffer ("fconn_read: Read", zbuf, *pclen);
  else if (FDEBUGGING (DEBUG_PORT))
    ulog (LOG_DEBUG, "fconn_read: Read %lu", (unsigned long) *pclen);
#endif

  return fret;
}

/* Write data to the connection.  */

boolean
fconn_write (qconn, zbuf, clen)
     struct sconnection *qconn;
     const char *zbuf;
     size_t clen;
{
#if DEBUG > 1
  if (FDEBUGGING (DEBUG_OUTGOING))
    udebug_buffer ("fconn_write: Writing", zbuf, clen);
  else if (FDEBUGGING (DEBUG_PORT))
    ulog (LOG_DEBUG, "fconn_write: Writing %lu", (unsigned long) clen);
#endif

  return (*qconn->qcmds->pfwrite) (qconn, zbuf, clen);
}

/* Read and write data.  */

boolean
fconn_io (qconn, zwrite, pcwrite, zread, pcread)
     struct sconnection *qconn;
     const char *zwrite;
     size_t *pcwrite;
     char *zread;
     size_t *pcread;
{
  boolean fret;
#if DEBUG > 1
  size_t cwrite = *pcwrite;
  size_t cread = *pcread;

  if (cread == 0 || cwrite == 0)
    ulog (LOG_FATAL, "fconn_io: cread %lu; cwrite %lu",
	  (unsigned long) cread, (unsigned long) cwrite);
#endif

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_OUTGOING))
    udebug_buffer ("fconn_io: Writing", zwrite, cwrite);
#endif

  fret = (*qconn->qcmds->pfio) (qconn, zwrite, pcwrite, zread, pcread);

  DEBUG_MESSAGE4 (DEBUG_PORT,
		  "fconn_io: Wrote %lu of %lu, read %lu of %lu",
		  (unsigned long) *pcwrite, (unsigned long) cwrite,
		  (unsigned long) *pcread, (unsigned long) cread);

#if DEBUG > 1
  if (*pcread > 0 && FDEBUGGING (DEBUG_INCOMING))
    udebug_buffer ("fconn_io: Read", zread, *pcread);
#endif

  return fret;
}

/* Send a break character to a connection.  Some port types may not
   support break characters, in which case we just return TRUE.  */

boolean
fconn_break (qconn)
     struct sconnection *qconn;
{
  boolean (*pfbreak) P((struct sconnection *));

  pfbreak = *qconn->qcmds->pfbreak;
  if (pfbreak == NULL)
    return TRUE;

  DEBUG_MESSAGE0 (DEBUG_PORT, "fconn_break: Sending break character");

  return (*pfbreak) (qconn);
}

/* Change the setting of a connection.  Some port types may not
   support this, in which case we just return TRUE.  */

boolean
fconn_set (qconn, tparity, tstrip, txonxoff)
     struct sconnection *qconn;
     enum tparitysetting tparity;
     enum tstripsetting tstrip;
     enum txonxoffsetting txonxoff;
{
  boolean (*pfset) P((struct sconnection *, enum tparitysetting,
		      enum tstripsetting, enum txonxoffsetting));

  pfset = qconn->qcmds->pfset;
  if (pfset == NULL)
    return TRUE;

  DEBUG_MESSAGE3 (DEBUG_PORT,
		  "fconn_set: Changing setting to %d, %d, %d",
		  (int) tparity, (int) tstrip, (int) txonxoff);

  return (*pfset) (qconn, tparity, tstrip, txonxoff);
}

/* Require or ignore carrier on a connection.  */

boolean
fconn_carrier (qconn, fcarrier)
     struct sconnection *qconn;
     boolean fcarrier;
{
  boolean (*pfcarrier) P((struct sconnection *, boolean));

  pfcarrier = qconn->qcmds->pfcarrier;
  if (pfcarrier == NULL)
    return TRUE;
  return (*pfcarrier) (qconn, fcarrier);
}

/* Run a chat program on a connection.  */

boolean
fconn_run_chat (qconn, pzprog)
     struct sconnection *qconn;
     char **pzprog;
{
  return (*qconn->qcmds->pfchat) (qconn, pzprog);
}

/* Get the baud rate of a connection.  */

long
iconn_baud (qconn)
     struct sconnection *qconn;
{
  long (*pibaud) P((struct sconnection *));

  pibaud = qconn->qcmds->pibaud;
  if (pibaud == NULL)
    return 0;
  return (*pibaud) (qconn);
}

/* Modem dialing routines.  */

/*ARGSUSED*/
boolean
fmodem_dial (qconn, puuconf, qsys, zphone, qdialer, ptdialerfound)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_system *qsys;
     const char *zphone;
     struct uuconf_dialer *qdialer;
     enum tdialerfound *ptdialerfound;
{
  *ptdialerfound = DIALERFOUND_FALSE;

  if (qconn->qport->uuconf_u.uuconf_smodem.uuconf_pzdialer != NULL)
    {
      char **pz;
      boolean ffirst;

      /* The pzdialer field is a sequence of dialer/token pairs.  The
	 dialer portion names a dialer to use.  The token portion is
	 what \D and \T in the chat script expand to.  If there is no
	 token for the last dialer, the phone number for the system is
	 used.  */
      ffirst = TRUE;
      pz = qconn->qport->uuconf_u.uuconf_smodem.uuconf_pzdialer;
      while (*pz != NULL)
	{
	  int iuuconf;
	  struct uuconf_dialer *q;
	  struct uuconf_dialer s;
	  const char *ztoken;
	  boolean ftranslate;

	  if (! ffirst)
	    q = &s;
	  else
	    q = qdialer;

	  iuuconf = uuconf_dialer_info (puuconf, *pz, q);
	  if (iuuconf == UUCONF_NOT_FOUND)
	    {
	      ulog (LOG_ERROR, "%s: Dialer not found", *pz);
	      return FALSE;
	    }
	  else if (iuuconf != UUCONF_SUCCESS)
	    {
	      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	      return FALSE;
	    }

	  ++pz;
	  ztoken = *pz;

	  ftranslate = FALSE;
	  if (ztoken == NULL
	      || strcmp (ztoken, "\\D") == 0)
	    ztoken = zphone;
	  else if (strcmp (ztoken, "\\T") == 0)
	    {
	      ztoken = zphone;
	      ftranslate = TRUE;
	    }

	  if (! fcdo_dial (qconn, puuconf, q, ztoken, ftranslate))
	    {
	      (void) uuconf_dialer_free (puuconf, q);
	      if (! ffirst)
		(void) uuconf_dialer_free (puuconf, qdialer);
	      return FALSE;
	    }

	  if (ffirst)
	    {
	      *ptdialerfound = DIALERFOUND_FREE;
	      ffirst = FALSE;
	    }
	  else
	    (void) uuconf_dialer_free (puuconf, q);

	  if (*pz != NULL)
	    ++pz;
	}

      return TRUE;
    }
  else if (qconn->qport->uuconf_u.uuconf_smodem.uuconf_qdialer != NULL)
    {
      struct uuconf_dialer *q;

      q = qconn->qport->uuconf_u.uuconf_smodem.uuconf_qdialer;
      *qdialer = *q;
      *ptdialerfound = DIALERFOUND_TRUE;
      return fcdo_dial (qconn, puuconf, q, zphone, FALSE);
    }
  else
    {
      ulog (LOG_ERROR, "No dialer information");
      return FALSE;
    }
}

/* Actually use a dialer.  We set up the modem (which may include
   opening the dialer device), run the chat script, and finish dealing
   with the modem.  */

static boolean
fcdo_dial (qconn, puuconf, qdial, zphone, ftranslate)
     struct sconnection *qconn;
     pointer puuconf;
     struct uuconf_dialer *qdial;
     const char *zphone;
     boolean ftranslate;
{
  const char *zname;

  if (! fsysdep_modem_begin_dial (qconn, qdial))
    return FALSE;

  if (qconn->qport == NULL)
    zname = NULL;
  else
    zname = qconn->qport->uuconf_zname;

  if (! fchat (qconn, puuconf, &qdial->uuconf_schat,
	       (const struct uuconf_system *) NULL, qdial,
	       zphone, ftranslate, zname, iconn_baud (qconn)))
    return FALSE;

  return fsysdep_modem_end_dial (qconn, qdial);
}
