/* prote.c
   The 'e' protocol.

   Copyright (C) 1991, 1992, 1995 Ian Lance Taylor

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
const char prote_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* This implementation is based on my implementation of the 't'
   protocol, which is fairly similar.  The main difference between the
   protocols seems to be that 't' breaks the file into packets and
   transmits the size of the packet with each packet, whereas 'e'
   sends the size of the entire file and then sends all the data in a
   single enormous packet.

   The 'e' protocol does no error checking whatsoever and thus
   requires an end-to-end verified eight bit communication line, such
   as is provided by TCP.  Using it with a modem is inadvisable, since
   errors can occur between the modem and the computer.  */

/* The buffer size we use.  */
#define CEBUFSIZE (CRECBUFLEN / 2)

/* The size of the initial file size message.  */
#define CEFRAMELEN (20)

/* A pointer to the buffer we will use.  */
static char *zEbuf;

/* True if we are receiving a file.  */
static boolean fEfile;

/* The number of bytes we have left to send or receive.  */
static long cEbytes;

/* The timeout we use.  */
static int cEtimeout = 120;

struct uuconf_cmdtab asEproto_params[] =
{
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cEtimeout, NULL },
  { NULL, 0, NULL, NULL }
};

/* Local function.  */

static boolean feprocess_data P((struct sdaemon *qdaemon, boolean *pfexit,
				 size_t *pcneed));

/* Start the protocol.  */

boolean
festart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  *pzlog = NULL;
  if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
		   STRIPSETTING_EIGHTBITS, XONXOFF_OFF))
    return FALSE;
  zEbuf = (char *) xmalloc (CEBUFSIZE);
  fEfile = FALSE;
  usysdep_sleep (2);
  return TRUE;
}

/* Stop the protocol.  */

/*ARGSUSED*/
boolean 
feshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  xfree ((pointer) zEbuf);
  zEbuf = NULL;
  cEtimeout = 120;
  return TRUE;
}

/* Send a command string.  We send everything up to and including the
   null byte.   */

/*ARGSUSED*/
boolean
fesendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fesendcmd: Sending command \"%s\"", z);

  return fsend_data (qdaemon->qconn, z, strlen (z) + 1, TRUE);
}

/* Get space to be filled with data.  We provide a buffer which has
   20 bytes at the start available to hold the length.  */

/*ARGSUSED*/
char *
zegetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = CEBUFSIZE;
  return zEbuf;
}

/* Send out some data.  We are allowed to modify the 20 bytes
   preceding the buffer.  This allows us to send the entire block with
   header bytes in a single call.  */

/*ARGSIGNORED*/
boolean
fesenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
#if DEBUG > 0
  /* Keep track of the number of bytes we send out to make sure it all
     adds up.  */
  cEbytes -= cdata;
  if (cEbytes < 0)
    {
      ulog (LOG_ERROR, "Protocol 'e' internal error");
      return FALSE;
    }
#endif

  /* We pass FALSE to fsend_data since we don't expect the other side
     to be sending us anything just now.  */
  return fsend_data (qdaemon->qconn, zdata, cdata, FALSE);
}

/* Process data and return the amount we need in *pfneed.  */

static boolean
feprocess_data (qdaemon, pfexit, pcneed)
     struct sdaemon *qdaemon;
     boolean *pfexit;
     size_t *pcneed;
{
  int cinbuf, cfirst, clen;

  *pfexit = FALSE;

  cinbuf = iPrecend - iPrecstart;
  if (cinbuf < 0)
    cinbuf += CRECBUFLEN;

  if (! fEfile)
    {
      /* We are not receiving a file.  Commands continue up to a null
	 byte.  */
      while (cinbuf > 0)
	{
	  char *pnull;

	  cfirst = CRECBUFLEN - iPrecstart;
	  if (cfirst > cinbuf)
	    cfirst = cinbuf;

	  pnull = memchr (abPrecbuf + iPrecstart, '\0', (size_t) cfirst);
	  if (pnull != NULL)
	    cfirst = pnull - (abPrecbuf + iPrecstart) + 1;

	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "feprocess_data: Got %d command bytes",
			  cfirst);

	  if (! fgot_data (qdaemon, abPrecbuf + iPrecstart,
			   (size_t) cfirst, (const char *) NULL, (size_t) 0,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;

	  iPrecstart = (iPrecstart + cfirst) % CRECBUFLEN;

	  if (*pfexit)
	    return TRUE;

	  cinbuf = iPrecend - iPrecstart;
	  if (cinbuf < 0)
	    cinbuf += CRECBUFLEN;
	}

      if (pcneed != NULL)
	*pcneed = 1;

      return TRUE;
    }

  /* Here we are receiving a file.  We want cEbytes in total.  If we
     don't have cEbytes yet, we have to get it first.  */

  if (cEbytes == -1)
    {
      char ab[CEFRAMELEN + 1];

      if (cinbuf < CEFRAMELEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CEFRAMELEN - cinbuf;
	  return TRUE;
	}

      cfirst = CRECBUFLEN - iPrecstart;
      if (cfirst >= CEFRAMELEN)
	memcpy (ab, abPrecbuf + iPrecstart, (size_t) CEFRAMELEN);
      else
	{
	  memcpy (ab, abPrecbuf + iPrecstart, (size_t) cfirst);
	  memcpy (ab + cfirst, abPrecbuf, (size_t) CEFRAMELEN - cfirst);
	}

      ab[CEFRAMELEN] = '\0';
      cEbytes = strtol (ab, (char **) NULL, 10);

      iPrecstart = (iPrecstart + CEFRAMELEN) % CRECBUFLEN;

      cinbuf = iPrecend - iPrecstart;
      if (cinbuf < 0)
	cinbuf += CRECBUFLEN;

      if (cEbytes == 0)
	{
	  if (! fgot_data (qdaemon, abPrecbuf, (size_t) 0,
			   (const char *) NULL, (size_t) 0,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;
	  if (*pfexit)
	    return TRUE;
	}
    }

  /* Here we can read real data for the file.  */

  while (cinbuf > 0)
    {
      clen = cinbuf;
      if ((long) clen > cEbytes)
	clen = (int) cEbytes;

      cfirst = CRECBUFLEN - iPrecstart;
      if (cfirst > clen)
	cfirst = clen;

      DEBUG_MESSAGE1 (DEBUG_PROTO,
		      "feprocess_data: Got %d data bytes",
		      clen);

      if (! fgot_data (qdaemon, abPrecbuf + iPrecstart,
		       (size_t) cfirst, abPrecbuf, (size_t) (clen - cfirst),
		       -1, -1, (long) -1, TRUE, pfexit))
	return FALSE;

      iPrecstart = (iPrecstart + clen) % CRECBUFLEN;
      cEbytes -= clen;

      if (cEbytes == 0)
	{
	  if (! fgot_data (qdaemon, abPrecbuf, (size_t) 0,
			   (const char *) NULL, (size_t) 0,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;
	  if (*pfexit)
	    return TRUE;
	}

      cinbuf -= clen;
    }

  if (pcneed != NULL)
    {
      if (cEbytes > CRECBUFLEN / 2)
	*pcneed = CRECBUFLEN / 2;
      else
	*pcneed = (int) cEbytes;
    }

  return TRUE;
}

/* Wait for data to come in and process it until we've reached the end
   of a command or a file.  */

boolean
fewait (qdaemon)
     struct sdaemon *qdaemon;
{
  while (TRUE)
    {
      boolean fexit;
      size_t cneed, crec;

      if (! feprocess_data (qdaemon, &fexit, &cneed))
	return FALSE;
      if (fexit)
	return TRUE;

      if (! freceive_data (qdaemon->qconn, cneed, &crec, cEtimeout, TRUE))
	return FALSE;

      if (crec == 0)
	{
	  ulog (LOG_ERROR, "Timed out waiting for data");
	  return FALSE;
	}
    }
}

/* File level routine, to handle transferring the amount of data and
   to set fEfile correctly.  */

boolean
fefile (qdaemon, qtrans, fstart, fsend, cbytes, pfhandled)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
     boolean fstart;
     boolean fsend;
     long cbytes;
     boolean *pfhandled;
{
  *pfhandled = FALSE;

  if (fstart)
    {
      if (fsend)
	{
	  char ab[CEFRAMELEN];

	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "Protocol 'e' starting to send %ld bytes",
			  cbytes);

	  bzero (ab, (size_t) CEFRAMELEN);
	  sprintf (ab, "%ld", cbytes);
	  if (! fsend_data (qdaemon->qconn, ab, (size_t) CEFRAMELEN, TRUE))
	    return FALSE;
	  cEbytes = cbytes;
	}
      else
	{
	  cEbytes = -1;
	  fEfile = TRUE;
	}
    }
  else
    {
      if (! fsend)
	fEfile = FALSE;
#if DEBUG > 0
      if (cEbytes != 0)
	{
	  ulog (LOG_ERROR,
		"Protocol 'e' internal error: %ld bytes left over",
		cEbytes);
	  return FALSE;
	}
#endif
    }

  return TRUE;
}
