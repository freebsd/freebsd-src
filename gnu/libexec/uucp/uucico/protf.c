/* protf.c
   The 'f' protocol.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char protf_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucico/protf.c,v 1.7 1999/08/27 23:33:47 peter Exp $";
#endif

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* This implementation is based on code by Piet Beertema, CWI,
   Amsterdam, Sep 1984.

   This code implements the 'f' protocol, which requires a
   flow-controlled error-free seven-bit data path.  It does check for
   errors, but only at the end of each file transmission, so a noisy
   line without error correcting modems will be unusable.

   The conversion to seven bit data is done as follows, where b
   represents the character to convert:

      0 <= b <=  037: 0172, b + 0100 (0100 to 0137)
    040 <= b <= 0171:       b        ( 040 to 0171)
   0172 <= b <= 0177: 0173, b - 0100 ( 072 to 077)
   0200 <= b <= 0237: 0174, b - 0100 (0100 to 0137)
   0240 <= b <= 0371: 0175, b - 0200 ( 040 to 0171)
   0372 <= b <= 0377: 0176, b - 0300 ( 072 to 077)

   This causes all output bytes to be in the range 040 to 0176; these
   are the printable ASCII characters.  */

/* This structure is used to hold information when dealing with the
   end of file acknowledgement.  */

struct sfinfo
{
  /* The functions from the generic code.  */
  boolean (*psendfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon));
  boolean (*precfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon,
		       const char *zdata, size_t cdata));
  /* The info pointer from the generic code.  */
  pointer pinfo;
  /* The character to send after receiving the checksum.  */
  char bsend;
};

/* Internal functions.  */
static boolean ffprocess_data P((struct sdaemon *qdaemon,
				 boolean *pfexit, size_t *pcneed));
static boolean ffawait_ack P((struct stransfer *qtrans,
			      struct sdaemon *qdaemon,
			      const char *zdata, size_t cdata));
static boolean ffawait_cksum P((struct stransfer *qtrans,
				struct sdaemon *qdaemon,
				const char *zdata, size_t cdata));
static boolean ffsend_ack P((struct stransfer *qtrans,
			     struct sdaemon *qdaemon));

/* The size of the buffer we allocate to store outgoing data in.  */
#define CFBUFSIZE (256)

/* The timeout to wait for data to arrive before giving up.  */
static int cFtimeout = 120;

/* The maximum number of retries.  */
static int cFmaxretries = 2;

/* The buffer we allocate for outgoing data.  */
static char *zFbuf;

/* TRUE if we are receiving a file rather than a command.  */
static boolean fFfile;

/* The checksum so far.  */
static unsigned int iFcheck;

/* The last special byte (0172 to 0176) or 0 if none.  */
static char bFspecial;

/* The number of times we have retried this file.  */
static int cFretries;

/* Whether this file has been acknowledged.  */
static boolean fFacked;

struct uuconf_cmdtab asFproto_params[] =
{
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cFtimeout, NULL },
  { "retries", UUCONF_CMDTABTYPE_INT, (pointer) &cFmaxretries, NULL },
  { NULL, 0, NULL, NULL }
};

/* Statistics.  */

/* The number of data bytes sent in files.  */
static long cFsent_data;

/* The number of actual bytes sent in files.  */
static long cFsent_bytes;

/* The number of data bytes received in files.  */
static long cFrec_data;

/* The number of actual bytes received in files.  */
static long cFrec_bytes;

/* The number of file retries when sending.  */
static long cFsend_retries;

/* The number of file retries when receiving.  */
static long cFrec_retries;

/* Start the protocol.  */

boolean
ffstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  *pzlog = NULL;

  cFsent_data = 0;
  cFsent_bytes = 0;
  cFrec_data = 0;
  cFrec_bytes = 0;
  cFsend_retries = 0;
  cFrec_retries = 0;

  /* Use XON/XOFF handshaking.  */
  if (! fconn_set (qdaemon->qconn, PARITYSETTING_DEFAULT,
		   STRIPSETTING_SEVENBITS, XONXOFF_ON))
    return FALSE;

  /* We sleep to allow the other side to reset the terminal; this is
     what Mr. Beertema's code does.  */
  usysdep_sleep (2);

  return TRUE;
}

/* Shutdown the protocol.  */

/*ARGSIGNORED*/
boolean
ffshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  xfree ((pointer) zFbuf);
  zFbuf = NULL;
  ulog (LOG_NORMAL,
	"Protocol 'f': sent %ld bytes for %ld, received %ld bytes for %ld",
	cFsent_bytes, cFsent_data, cFrec_bytes, cFrec_data);
  if (cFsend_retries != 0 || cFrec_retries != 0)
    ulog (LOG_NORMAL, "Protocol 'f' file retries: %ld sending, %ld receiving",
	  cFsend_retries, cFrec_retries);
  cFtimeout = 120;
  cFmaxretries = 2;
  return TRUE;
}

/* Send a command string.  We just send the string followed by a carriage
   return.  */

/*ARGSUSED*/
boolean
ffsendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  size_t clen;
  char *zalc;
  boolean fret;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "ffsendcmd: Sending command \"%s\"", z);

  clen = strlen (z);
  zalc = zbufalc (clen + 2);
  memcpy (zalc, z, clen);
  zalc[clen] = '\r';
  zalc[clen + 1] = '\0';
  fret = fsend_data (qdaemon->qconn, zalc, clen + 1, TRUE);
  ubuffree (zalc);
  return fret;
}

/* Get space to be filled with data.  We allocate the space from the
   heap.  */

/*ARGSIGNORED*/
char *
zfgetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = CFBUFSIZE;
  if (zFbuf == NULL)
    zFbuf = (char *) xmalloc (CFBUFSIZE);
  return zFbuf;
}

/* Send out a data packet.  We have to encode the data into seven bits
   and accumulate a checksum.  */

/*ARGSIGNORED*/
boolean
ffsenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
  char ab[CFBUFSIZE * 2];
  char *ze;
  register unsigned int itmpchk;
      
  cFsent_data += cdata;

  ze = ab;
  itmpchk = iFcheck;
  while (cdata-- > 0)
    {
      register int b;

      /* Rotate the checksum left.  */
      if ((itmpchk & 0x8000) == 0)
	itmpchk <<= 1;
      else
	{
	  itmpchk <<= 1;
	  ++itmpchk;
	}

      /* Add the next byte into the checksum.  */
      b = *zdata++ & 0xff;
      itmpchk += b;

      /* Encode the byte.  */
      if (b <= 0177)
	{
	  if (b <= 037)
	    {
	      *ze++ = '\172';
	      *ze++ = (char) (b + 0100);
	    }
	  else if (b <= 0171)
	    *ze++ = (char) b;
	  else
	    {
	      *ze++ = '\173';
	      *ze++ = (char) (b - 0100);
	    }
	}
      else
	{
	  if (b <= 0237)
	    {
	      *ze++ = '\174';
	      *ze++ = (char) (b - 0100);
	    }
	  else if (b <= 0371)
	    {
	      *ze++ = '\175';
	      *ze++ = (char) (b - 0200);
	    }
	  else
	    {
	      *ze++ = '\176';
	      *ze++ = (char) (b - 0300);
	    }
	}
    }

  iFcheck = itmpchk;

  cFsent_bytes += ze - ab;

  /* Passing FALSE tells fsend_data not to bother looking for incoming
     information, since we really don't expect any.  */
  return fsend_data (qdaemon->qconn, ab, (size_t) (ze - ab), FALSE);
}

/* Process data and return the amount of data we are looking for in
   *pcneed.  The 'f' protocol doesn't really reveal this, but when
   transferring file we know that we need at least seven characters
   for the checksum.  */

static boolean
ffprocess_data (qdaemon, pfexit, pcneed)
     struct sdaemon *qdaemon;
     boolean *pfexit;
     size_t *pcneed;
{
  int i;
  register unsigned int itmpchk;

  *pfexit = FALSE;
  if (pcneed != NULL)
    *pcneed = 1;

  if (! fFfile)
    {
      /* A command continues until a '\r' character, which we turn
	 into '\0' before calling fgot_data.  */
      while (iPrecstart != iPrecend)
	{
	  for (i = iPrecstart; i < CRECBUFLEN && i != iPrecend; i++)
	    {
	      /* Some systems seem to send characters with parity, so
		 strip the parity bit.  */
	      abPrecbuf[i] &= 0x7f;

	      if (abPrecbuf[i] == '\r')
		{
		  int istart;

		  DEBUG_MESSAGE1 (DEBUG_PROTO,
				  "ffprocess_data: Got %d command bytes",
				  i - iPrecstart + 1);

		  abPrecbuf[i] = '\0';
		  istart = iPrecstart;
		  iPrecstart = (i + 1) % CRECBUFLEN;
		  if (pcneed != NULL)
		    *pcneed = 0;
		  return fgot_data (qdaemon, abPrecbuf + istart,
				    (size_t) (i - istart + 1),
				    (const char *) NULL, (size_t) 0,
				    -1, -1, (long) -1, TRUE, pfexit);
		}
	    }

	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "ffprocess_data: Got %d command bytes",
			  i - iPrecstart);

	  if (! fgot_data (qdaemon, abPrecbuf + iPrecstart,
			   (size_t) (i - iPrecstart),
			   (const char *) NULL, (size_t) 0,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;

	  iPrecstart = i % CRECBUFLEN;
	}

      return TRUE;
    }

  /* Here the data is destined for a file, and we must decode it.  */

  itmpchk = iFcheck;

  while (iPrecstart != iPrecend)
    {
      char *zstart, *zto, *zfrom;
      int c;

      zto = zfrom = zstart = abPrecbuf + iPrecstart;

      c = iPrecend - iPrecstart;
      if (c < 0)
	c = CRECBUFLEN - iPrecstart;

      while (c-- != 0)
	{
	  int b;

	  /* Some systems seem to send characters with parity, so
	     strip the parity bit.  */
	  b = *zfrom++ & 0x7f;
	  if (b < 040 || b > 0176)
	    {
	      ulog (LOG_ERROR, "Illegal byte %d", b);
	      continue;
	    }

	  /* Characters >= 0172 are always special characters.  The
	     only legal pair of consecutive special characters
	     are 0176 0176 which immediately precede the four
	     digit checksum.  */
	  if (b >= 0172)
	    {
	      if (bFspecial != 0)
		{
		  if (bFspecial != 0176 || b != 0176)
		    {
		      ulog (LOG_ERROR, "Illegal bytes %d %d",
			    bFspecial, b);
		      bFspecial = 0;
		      continue;
		    }

		  /* Pass any initial data.  */
		  if (zto != zstart)
		    {
		      /* Don't count the checksum in the received bytes.  */
		      cFrec_bytes += zfrom - zstart - 2;
		      cFrec_data += zto - zstart;
		      if (! fgot_data (qdaemon, zstart,
				       (size_t) (zto - zstart),
				       (const char *) NULL, (size_t) 0,
				       -1, -1, (long) -1, TRUE, pfexit))
			return FALSE;
		    }

		  /* The next characters we want to read are the
		     checksum, so skip the second 0176.  */
		  iPrecstart = (iPrecstart + zfrom - zstart) % CRECBUFLEN;

		  iFcheck = itmpchk;

		  /* Tell fgot_data that we've read the entire file by
		     passing 0 length data.  This will wind up calling
		     fffile to verify the checksum.  We set *pcneed to
		     0 because we don't want to read any more data
		     from the port, since we may have already read the
		     checksum.  */
		  if (pcneed != NULL)
		    *pcneed = 0;
		  return fgot_data (qdaemon, (const char *) NULL,
				    (size_t) 0, (const char *) NULL,
				    (size_t) 0, -1, -1, (long) -1,
				    TRUE, pfexit);
		}

	      /* Here we have encountered a special character that
		 does not follow another special character.  */
	      bFspecial = (char) b;
	    }
	  else
	    {
	      int bnext;

	      /* Here we have encountered a nonspecial character.  */

	      switch (bFspecial)
		{
		default:
		  bnext = b;
		  break;
		case 0172:
		  bnext = b - 0100;
		  break;
		case 0173:
		case 0174:
		  bnext = b + 0100;
		  break;
		case 0175:
		  bnext = b + 0200;
		  break;
		case 0176:
		  bnext = b + 0300;
		  break;
		}

	      *zto++ = (char) bnext;
	      bFspecial = 0;

	      /* Rotate the checksum left.  */
	      if ((itmpchk & 0x8000) == 0)
		itmpchk <<= 1;
	      else
		{
		  itmpchk <<= 1;
		  ++itmpchk;
		}

	      /* Add the next byte into the checksum.  */
	      itmpchk += bnext;
	    }
	}

      if (zto != zstart)
	{
	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "ffprocess_data: Got %d bytes",
			  zto - zstart);

	  cFrec_data += zto - zstart;
	  if (! fgot_data (qdaemon, zstart, (size_t) (zto - zstart),
			   (const char *) NULL, (size_t) 0,
			   -1, -1, (long) -1, TRUE, pfexit))
	    return FALSE;
	}

      cFrec_bytes += zfrom - zstart;

      iPrecstart = (iPrecstart + zfrom - zstart) % CRECBUFLEN;
    }

  iFcheck = itmpchk;

  if (pcneed != NULL)
    {
      /* At this point we may have seen the first 0176 in the checksum
	 but not the second.  The checksum is at least seven
	 characters long (0176 0176 a b c d \r).  This won't help
	 much, but reading seven characters is a lot better than
	 reading two, which is what I saw in a 2400 baud log file.  */
      if (bFspecial == 0176)
	*pcneed = 6;
      else
	*pcneed = 7;
    }

  return TRUE;
}

/* Wait for data to come in and process it until we've finished a
   command or a file.  */

boolean
ffwait (qdaemon)
     struct sdaemon *qdaemon;
{
  while (TRUE)
    {
      boolean fexit;
      size_t cneed, crec;

      if (! ffprocess_data (qdaemon, &fexit, &cneed))
	return FALSE;
      if (fexit)
	return TRUE;

      if (cneed > 0)
	{
	  /* We really want to do something like get all available
	     characters, then sleep for half a second and get all
	     available characters again, and keep this up until we
	     don't get anything after sleeping.  */
	  if (! freceive_data (qdaemon->qconn, cneed, &crec, cFtimeout, TRUE))
	    return FALSE;
	  if (crec == 0)
	    {
	      ulog (LOG_ERROR, "Timed out waiting for data");
	      return FALSE;
	    }
	}
    }
}

/* File level operations.  Reset the checksums when starting to send
   or receive a file, and output the checksum when we've finished
   sending a file.  */

/*ARGSUSED*/
boolean
fffile (qdaemon, qtrans, fstart, fsend, cbytes, pfhandled)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
     boolean fstart;
     boolean fsend;
     long cbytes;
     boolean *pfhandled;
{
  DEBUG_MESSAGE3 (DEBUG_PROTO, "fffile: fstart %s; fsend %s; fFacked %s",
		  fstart ? "true" : "false", fsend ? "true" : "false",
		  fFacked ? "true" : "false");

  *pfhandled = FALSE;

  if (fstart)
    {
      iFcheck = 0xffff;
      cFretries = 0;
      fFacked = FALSE;
      if (! fsend)
	{
	  bFspecial = 0;
	  fFfile = TRUE;
	}
      return TRUE;
    }
  else
    {
      struct sfinfo *qinfo;

      /* We need to handle the checksum and the acknowledgement.  If
	 we get a successful ACK, we set fFacked to TRUE and call the
	 send or receive function by hand.  This will wind up calling
	 here again, so if fFacked is TRUE we just return out and let
	 the send or receive function do whatever it does.  This is a
	 bit of a hack.  */
      if (fFacked)
	{
	  fFacked = FALSE;
	  return TRUE;
	}

      if (fsend)
	{
	  char ab[sizeof "\176\176ABCD\r"];

	  /* Send the final checksum.  */
	  sprintf (ab, "\176\176%04x\r", iFcheck & 0xffff);
	  if (! fsend_data (qdaemon->qconn, ab, (size_t) 7, TRUE))
	    return FALSE;

	  /* Now wait for the acknowledgement.  */
	  fFfile = FALSE;
	  qinfo = (struct sfinfo *) xmalloc (sizeof (struct sfinfo));
	  qinfo->psendfn = qtrans->psendfn;
	  qinfo->precfn = qtrans->precfn;
	  qinfo->pinfo = qtrans->pinfo;
	  qtrans->psendfn = NULL;
	  qtrans->precfn = ffawait_ack;
	  qtrans->pinfo = (pointer) qinfo;
	  qtrans->fcmd = TRUE;

	  *pfhandled = TRUE;

	  return fqueue_receive (qdaemon, qtrans);
	}
      else
	{
	  /* Wait for the checksum.  */
	  fFfile = FALSE;
	  qinfo = (struct sfinfo *) xmalloc (sizeof (struct sfinfo));
	  qinfo->psendfn = qtrans->psendfn;
	  qinfo->precfn = qtrans->precfn;
	  qinfo->pinfo = qtrans->pinfo;
	  qtrans->psendfn = NULL;
	  qtrans->precfn = ffawait_cksum;
	  qtrans->pinfo = (pointer) qinfo;
	  qtrans->fcmd = TRUE;

	  *pfhandled = TRUE;

	  return fqueue_receive (qdaemon, qtrans);
	}
    }
}

/* Wait for the ack after sending a file and the checksum.  */

static boolean
ffawait_ack (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct sfinfo *qinfo = (struct sfinfo *) qtrans->pinfo;

  qtrans->precfn = NULL;

  /* An R means to retry sending the file.  */
  if (*zdata == 'R')
    {
      if (! ffileisopen (qtrans->e))
	{
	  ulog (LOG_ERROR, "Request to resent non-file");
	  return FALSE;
	}

      ++cFretries;
      if (cFretries > cFmaxretries)
	{
	  ulog (LOG_ERROR, "Too many retries");
	  return FALSE;
	}

      ulog (LOG_NORMAL, "Resending file");
      if (! ffilerewind (qtrans->e))
	{
	  ulog (LOG_ERROR, "rewind: %s", strerror (errno));
	  return FALSE;
	}
      qtrans->ipos = (long) 0;

      iFcheck = 0xffff;
      ++cFsend_retries;

      qtrans->psendfn = qinfo->psendfn;
      qtrans->precfn = qinfo->precfn;
      qtrans->pinfo = qinfo->pinfo;
      xfree ((pointer) qinfo);
      qtrans->fsendfile = TRUE;

      return fqueue_send (qdaemon, qtrans);
    }

  if (*zdata != 'G')
    {
      DEBUG_MESSAGE1 (DEBUG_PROTO, "fffile: Got \"%s\"", zdata);
      ulog (LOG_ERROR, "File send failed");
      return FALSE;
    }

  qtrans->psendfn = qinfo->psendfn;
  qtrans->precfn = qinfo->precfn;
  qtrans->pinfo = qinfo->pinfo;
  xfree ((pointer) qinfo);

  /* Now call the send function by hand after setting fFacked to TRUE.
     Since fFacked is true fffile will simply return out, and the send
     function can do whatever it what was going to do.  */
  fFacked = TRUE;
  return (*qtrans->psendfn) (qtrans, qdaemon);
}

/* This function is called when the checksum arrives.  */

/*ARGSUSED*/
static boolean
ffawait_cksum (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct sfinfo *qinfo = (struct sfinfo *) qtrans->pinfo;
  unsigned int icheck;

  qtrans->precfn = NULL;

  if (! isxdigit (zdata[0])
      || ! isxdigit (zdata[1])
      || ! isxdigit (zdata[2])
      || ! isxdigit (zdata[3])
      || zdata[4] != '\0')
    {
      ulog (LOG_ERROR, "Bad checksum format");
      xfree (qtrans->pinfo);
      return FALSE;
    }
	  
  icheck = (unsigned int) strtol ((char *) zdata, (char **) NULL, 16);

  if (icheck != (iFcheck & 0xffff))
    {
      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
		      "Checksum failed; calculated 0x%x, got 0x%x",
		      iFcheck & 0xffff, icheck);

      if (! ffileisopen (qtrans->e))
	{
	  ulog (LOG_ERROR, "Failed to get non-file");
	  return FALSE;
	}

      ++cFretries;
      if (cFretries > cFmaxretries)
	{
	  ulog (LOG_ERROR, "Too many retries");
	  qinfo->bsend = 'Q';
	}
      else
	{
	  ulog (LOG_NORMAL, "File being resent");

	  /* This bit of code relies on the receive code setting
	     qtrans->s.ztemp to the full name of the temporary file
	     being used.  */
	  qtrans->e = esysdep_truncate (qtrans->e, qtrans->s.ztemp);
	  if (! ffileisopen (qtrans->e))
	    return FALSE;
	  qtrans->ipos = (long) 0;

	  iFcheck = 0xffff;
	  bFspecial = 0;
	  fFfile = TRUE;
	  ++cFrec_retries;

	  /* Send an R to tell the other side to resend the file.  */
	  qinfo->bsend = 'R';
	}
    }
  else
    {
      /* Send a G to tell the other side the file was received
	 correctly.  */
      qinfo->bsend = 'G';
    }

  qtrans->psendfn = ffsend_ack;

  return fqueue_send (qdaemon, qtrans);
}

/* Send the acknowledgement, and then possible wait for the resent
   file.  */

static boolean
ffsend_ack (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct sfinfo *qinfo = (struct sfinfo *) qtrans->pinfo;
  char ab[2];

  ab[0] = qinfo->bsend;
  ab[1] = '\0';
  if (! ffsendcmd (qdaemon, ab, 0, 0))
    return FALSE;

  qtrans->psendfn = qinfo->psendfn;
  qtrans->precfn = qinfo->precfn;
  qtrans->pinfo = qinfo->pinfo;
  xfree ((pointer) qinfo);

  if (ab[0] == 'Q')
    return FALSE;
  if (ab[0] == 'R')
    {
      qtrans->frecfile = TRUE;
      return fqueue_receive (qdaemon, qtrans);
    }

  fFacked = TRUE;
  return (*qtrans->precfn) (qtrans, qdaemon, (const char *) NULL,
			    (size_t) 0);
}
