/* prott.c
   The 't' protocol.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char prott_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* This implementation is based on code written by Rick Adams.

   This code implements the 't' protocol, which does no error checking
   whatsoever and thus requires an end-to-end verified eight bit
   communication line, such as is provided by TCP.  Using it with a
   modem is unadvisable, since errors can occur between the modem and
   the computer.  */

/* The buffer size we use.  */
#define CTBUFSIZE (1024)

/* The offset in the buffer to the data.  */
#define CTFRAMELEN (4)

/* Commands are sent in multiples of this size.  */
#define CTPACKSIZE (512)

/* A pointer to the buffer we will use.  */
static char *zTbuf;

/* True if we are receiving a file.  */
static boolean fTfile;

/* The timeout we use.  */
static int cTtimeout = 120;

struct uuconf_cmdtab asTproto_params[] =
{
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cTtimeout, NULL },
  { NULL, 0, NULL, NULL }
};

/* Local function.  */

static boolean ftprocess_data P((struct sdaemon *qdaemon, boolean *pfexit,
				 size_t *pcneed));

/* Start the protocol.  */

boolean
ftstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  *pzlog = NULL;
  if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
		   STRIPSETTING_EIGHTBITS, XONXOFF_OFF))
    return FALSE;
  zTbuf = (char *) xmalloc (CTBUFSIZE + CTFRAMELEN);
  /* The first two bytes of the buffer are always zero.  */
  zTbuf[0] = 0;
  zTbuf[1] = 0;
  fTfile = FALSE;
  usysdep_sleep (2);
  return TRUE;
}

/* Stop the protocol.  */

/*ARGSUSED*/
boolean 
ftshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  xfree ((pointer) zTbuf);
  zTbuf = NULL;
  cTtimeout = 120;
  return TRUE;
}

/* Send a command string.  We send everything up to and including the
   null byte.  The number of bytes we send must be a multiple of
   TPACKSIZE.  */

/*ARGSUSED*/
boolean
ftsendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  size_t clen, csend;
  char *zalc;
  boolean fret;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "ftsendcmd: Sending command \"%s\"", z);

  clen = strlen (z);

  /* We need to send the smallest multiple of CTPACKSIZE which is
     greater than clen (not equal to clen, since we need room for the
     null byte).  */
  csend = ((clen / CTPACKSIZE) + 1) * CTPACKSIZE;

  zalc = zbufalc (csend);
  memcpy (zalc, z, clen);
  if (csend > clen)
    bzero (zalc + clen, csend - clen);

  fret = fsend_data (qdaemon->qconn, zalc, csend, TRUE);
  ubuffree (zalc);
  return fret;
}

/* Get space to be filled with data.  We provide a buffer which has
   four bytes at the start available to hold the length.  */

/*ARGSIGNORED*/
char *
ztgetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = CTBUFSIZE;
  return zTbuf + CTFRAMELEN;
}

/* Send out some data.  We are allowed to modify the four bytes
   preceding the buffer.  This allows us to send the entire block with
   header bytes in a single call.  */

/*ARGSIGNORED*/
boolean
ftsenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
  /* Here we do htonl by hand, since it doesn't exist everywhere.  We
     know that the amount of data cannot be greater than CTBUFSIZE, so
     the first two bytes of this value will always be 0.  They were
     set to 0 in ftstart so we don't touch them here.  This is useful
     because we cannot portably right shift by 24 or 16, since we
     might be dealing with sixteen bit integers.  */
  zdata[-2] = (char) ((cdata >> 8) & 0xff);
  zdata[-1] = (char) (cdata & 0xff);

  /* We pass FALSE to fsend_data since we don't expect the other side
     to be sending us anything just now.  */
  return fsend_data (qdaemon->qconn, zdata - CTFRAMELEN, cdata + CTFRAMELEN,
		     FALSE);
}

/* Process data and return the amount we need in *pfneed.  */

static boolean
ftprocess_data (qdaemon, pfexit, pcneed)
     struct sdaemon *qdaemon;
     boolean *pfexit;
     size_t *pcneed;
{
  int cinbuf, cfirst, clen;

  *pfexit = FALSE;

  cinbuf = iPrecend - iPrecstart;
  if (cinbuf < 0)
    cinbuf += CRECBUFLEN;

  if (! fTfile)
    {
      /* We are not receiving a file.  Commands are read in chunks of
	 CTPACKSIZE.  */
      while (cinbuf >= CTPACKSIZE)
	{
	  cfirst = CRECBUFLEN - iPrecstart;
	  if (cfirst > CTPACKSIZE)
	    cfirst = CTPACKSIZE;

	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "ftprocess_data: Got %d command bytes",
			  cfirst);

	  if (! fgot_data (qdaemon, abPrecbuf + iPrecstart,
			   (size_t) cfirst, abPrecbuf,
			   (size_t) CTPACKSIZE - cfirst,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;

	  iPrecstart = (iPrecstart + CTPACKSIZE) % CRECBUFLEN;

	  if (*pfexit)
	    return TRUE;

	  cinbuf -= CTPACKSIZE;
	}

      if (pcneed != NULL)
	*pcneed = CTPACKSIZE - cinbuf;

      return TRUE;
    }

  /* Here we are receiving a file.  The data comes in blocks.  The
     first four bytes contain the length, followed by that amount of
     data.  */

  while (cinbuf >= CTFRAMELEN)
    {
      /* The length is stored in network byte order, MSB first.  */

      clen = (((((((abPrecbuf[iPrecstart] & 0xff) << 8)
		  + (abPrecbuf[(iPrecstart + 1) % CRECBUFLEN] & 0xff)) << 8)
		+ (abPrecbuf[(iPrecstart + 2) % CRECBUFLEN] & 0xff)) << 8)
	      + (abPrecbuf[(iPrecstart + 3) % CRECBUFLEN] & 0xff));

      if (cinbuf < clen + CTFRAMELEN)
	{
	  if (pcneed != NULL)
	    *pcneed = clen + CTFRAMELEN - cinbuf;
	  return TRUE;
	}

      iPrecstart = (iPrecstart + CTFRAMELEN) % CRECBUFLEN;

      cfirst = CRECBUFLEN - iPrecstart;
      if (cfirst > clen)
	cfirst = clen;

      DEBUG_MESSAGE1 (DEBUG_PROTO,
		      "ftprocess_data: Got %d data bytes",
		      clen);

      if (! fgot_data (qdaemon, abPrecbuf + iPrecstart,
		       (size_t) cfirst, abPrecbuf, (size_t) (clen - cfirst),
		       -1, -1, (long) -1, TRUE, pfexit))
	return FALSE;

      iPrecstart = (iPrecstart + clen) % CRECBUFLEN;
			   
      if (*pfexit)
	return TRUE;

      cinbuf -= clen + CTFRAMELEN;
    }

  if (pcneed != NULL)
    *pcneed = CTFRAMELEN - cinbuf;

  return TRUE;
}

/* Wait for data to come in and process it until we've reached the end
   of a command or a file.  */

boolean
ftwait (qdaemon)
     struct sdaemon *qdaemon;
{
  while (TRUE)
    {
      boolean fexit;
      size_t cneed, crec;

      if (! ftprocess_data (qdaemon, &fexit, &cneed))
	return FALSE;
      if (fexit)
	return TRUE;

      if (! freceive_data (qdaemon->qconn, cneed, &crec, cTtimeout, TRUE))
	return FALSE;

      if (crec == 0)
	{
	  ulog (LOG_ERROR, "Timed out waiting for data");
	  return FALSE;
	}
    }
}

/* File level routine, to set fTfile correctly.  */

/*ARGSUSED*/
boolean
ftfile (qdaemon, qtrans, fstart, fsend, cbytes, pfhandled)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
     boolean fstart;
     boolean fsend;
     long cbytes;
     boolean *pfhandled;
{
  *pfhandled = FALSE;

  if (! fsend)
    fTfile = fstart;

  return TRUE;
}
