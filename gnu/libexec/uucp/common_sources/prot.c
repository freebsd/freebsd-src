/* prot.c
   Protocol support routines to move commands and data around.

   Copyright (C) 1991, 1992, 1994 Ian Lance Taylor

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
const char prot_rcsid[] = "$Id: prot.c,v 1.2 1994/05/07 18:08:51 ache Exp $";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "conn.h"
#include "prot.h"

/* Variables visible to the protocol-specific routines.  */

/* Buffer to hold received data.  */
char abPrecbuf[CRECBUFLEN];

/* Index of start of data in abPrecbuf.  */
int iPrecstart;

/* Index of end of data (first byte not included in data) in abPrecbuf.  */
int iPrecend;

/* We want to output and input at the same time, if supported on this
   machine.  If we have something to send, we send it all while
   accepting a large amount of data.  Once we have sent everything we
   look at whatever we have received.  If data comes in faster than we
   can send it, we may run out of buffer space.  */

boolean
fsend_data (qconn, zsend, csend, fdoread)
     struct sconnection *qconn;
     const char *zsend;
     size_t csend;
     boolean fdoread;
{
  if (! fdoread)
    return fconn_write (qconn, zsend, csend);

  while (csend > 0)
    {
      size_t crec, csent;

      if (iPrecend < iPrecstart)
	crec = iPrecstart - iPrecend - 1;
      else
	{
	  crec = CRECBUFLEN - iPrecend;
	  if (iPrecstart == 0)
	    --crec;
	}

      if (crec == 0)
	return fconn_write (qconn, zsend, csend);

      csent = csend;

      if (! fconn_io (qconn, zsend, &csent, abPrecbuf + iPrecend, &crec))
	return FALSE;

      csend -= csent;
      zsend += csent;

      iPrecend = (iPrecend + crec) % CRECBUFLEN;
    }

  return TRUE;
}

/* Read data from the other system when we have nothing to send.  The
   argument cneed is the amount of data the caller wants, and ctimeout
   is the timeout in seconds.  The function sets *pcrec to the amount
   of data which was actually received, which may be less than cneed
   if there isn't enough room in the receive buffer.  If no data is
   received before the timeout expires, *pcrec will be returned as 0.
   If an error occurs, the function returns FALSE.  If the freport
   argument is FALSE, no error should be reported.  */

boolean
freceive_data (qconn, cneed, pcrec, ctimeout, freport)
     struct sconnection *qconn;
     size_t cneed;
     size_t *pcrec;
     int ctimeout;
     boolean freport;
{
  /* Set *pcrec to the maximum amount of data we can read.  fconn_read
     expects *pcrec to be the buffer size, and sets it to the amount
     actually received.  */
  if (iPrecend < iPrecstart)
    *pcrec = iPrecstart - iPrecend - 1;
  else
    {
      *pcrec = CRECBUFLEN - iPrecend;
      if (iPrecstart == 0)
	--(*pcrec);
    }

#if DEBUG > 0
  /* If we have no room in the buffer, we're in trouble.  The
     protocols must be written to ensure that this can't happen.  */
  if (*pcrec == 0)
    ulog (LOG_FATAL, "freceive_data: No room in buffer");
#endif

  /* If we don't have room for all the data the caller wants, we
     simply have to expect less.  We'll get the rest later.  */
  if (*pcrec < cneed)
    cneed = *pcrec;

  if (! fconn_read (qconn, abPrecbuf + iPrecend, pcrec, cneed, ctimeout,
		    freport))
    return FALSE;

  iPrecend = (iPrecend + *pcrec) % CRECBUFLEN;

  return TRUE;
}

/* Read a single character.  Get it out of the receive buffer if it's
   there, otherwise ask freceive_data for at least one character.
   This is used because as a protocol is shutting down freceive_data
   may read ahead and eat characters that should be read outside the
   protocol routines.  We call freceive_data rather than fconn_read
   with an argument of 1 so that we can get all the available data in
   a single system call.  The ctimeout argument is the timeout in
   seconds; the freport argument is FALSE if no error should be
   reported.  This returns a character, or -1 on timeout or -2 on
   error.  */

int
breceive_char (qconn, ctimeout, freport)
     struct sconnection *qconn;
     int ctimeout;
     boolean freport;
{
  char b;

  if (iPrecstart == iPrecend)
    {
      size_t crec;

      if (! freceive_data (qconn, sizeof (char), &crec, ctimeout, freport))
	return -2;
      if (crec == 0)
	return -1;
    }

  b = abPrecbuf[iPrecstart];
  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
  return BUCHAR (b);
}

/* Send mail about a file transfer.  We send to the given mailing
   address if there is one, otherwise to the user.  */

boolean
fmail_transfer (fsuccess, zuser, zmail, zwhy, zfromfile, zfromsys,
		ztofile, ztosys, zsaved)
     boolean fsuccess;
     const char *zuser;
     const char *zmail;
     const char *zwhy;
     const char *zfromfile;
     const char *zfromsys;
     const char *ztofile;
     const char *ztosys;
     const char *zsaved;
{
  const char *zsendto;
  const char *az[20];
  int i;

  if (zmail != NULL && *zmail != '\0')
    zsendto = zmail;
  else
    zsendto = zuser;

  i = 0;
  az[i++] = "The file\n\t";
  if (zfromsys != NULL)
    {
      az[i++] = zfromsys;
      az[i++] = "!";
    }
  az[i++] = zfromfile;
  if (fsuccess)
    az[i++] = "\nwas successfully transferred to\n\t";
  else
    az[i++] = "\ncould not be transferred to\n\t";
  if (ztosys != NULL)
    {
      az[i++] = ztosys;
      az[i++] = "!";
    }
  az[i++] = ztofile;
  az[i++] = "\nas requested by\n\t";
  az[i++] = zuser;
  if (! fsuccess)
    {
      az[i++] = "\nfor the following reason:\n\t";
      az[i++] = zwhy;
      az[i++] = "\n";
    }
  if (zsaved != NULL)
    {
      az[i++] = zsaved;
      az[i++] = "\n";
    }

  return fsysdep_mail (zsendto,
		       fsuccess ? "UUCP succeeded" : "UUCP failed",
		       i, az);
}
