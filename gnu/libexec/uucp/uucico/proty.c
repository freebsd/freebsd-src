/* proty.c
   The 'y' protocol.

   Copyright (C) 1994, 1995 Jorge Cwik and Ian Lance Taylor

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
   */

#include "uucp.h"

#if USE_RCS_ID
const char proty_id[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* The 'y' protocol, and this implementation, was written and designed
   by Jorge Cwik <jorge@satlink.net>.  Some of the routines, and the
   coding style in general, were taken verbatim or adapted from other
   Taylor UUCP modules.  Mark Delany made the initial testings and
   helped in portability issues.

   This protocol does not perform any kind of error correction or flow
   control.  It does do error checking.  It does not require an end to
   end reliable link.  It is recommended for error-free (also called
   semi-reliable) connections as provided by error correcting modems.
   It needs an eight bit clean channel and some kind of flow control
   at the lower layers, typically RTS/CTS hardware flow control.

   The flow of the file transmission is completely unidirectional.
   There are no ACKs or NAKs outside file boundaries. This makes it
   very suitable for half duplex modulations (like PEP) and
   connections with very long delays, like multihop satellite links.  */

/* This protocol uses 16 bit little-endian ints in the packet header.  */
#define	FROMLITTLE(p)  (((p)[0] & 0xff) + (((p)[1] & 0xff) << 8))
#define	TOLITTLE(p, i) ((p)[0] = (i) & 0xff, (p)[1] = ((i) >> 8) & 0xff)

/* The buffer and packet size we use.  */
#define CYBUFSIZE (1024)
#define IYPACKSIZE (1024)

/* The offset in the buffer to the data.  */
#define CYFRAMELEN (6)

/* Offsets in a packet header.  */
#define YFRAME_SEQ_OFF (0)
#define YFRAME_LEN_OFF (2)
#define YFRAME_CTL_OFF (2)
#define YFRAME_CHK_OFF (4)

/* Offsets in a packet header viewed as an array of shorts.  */
#define YFRAME_SEQ (0)
#define YFRAME_LEN (1)
#define YFRAME_CTL (1)
#define YFRAME_CHK (2)

/* The default timeout.  */
#define	CYTIMEOUT (60)

/* Control packet types.  */
#define YPKT_ACK (0xFFFE)
#define YPKT_ERR (0xFFFD)
#define YPKT_BAD (0xFFFC)

/* The protocol version number.  */
#define Y_VERSION (1)

/* When the protocol starts up, it transmit the following information:
     1 byte version
     1 byte packet size
     2 byte flags (none currently defined)
   Future revision may expand the structure as long as these members
   keep their current offset.  */
#define Y_INIT_HDR_LEN (4)
#define Y_INIT_HDR_VERSION_OFF (0)
#define Y_INIT_HDR_PKTSIZE_OFF (1)
#define Y_INIT_HDR_FLAGS_OFF (2)

/* The initialization length of the lowest accepted version.  */
#define MIN_Y_SYNC (4)

/* Not strictly needed, but I would not want to accept a 32k sync pkt.  */
#define MAX_Y_SYNC IYPACKSIZE

/* Local and remote packet sizes (we actually use the same value for
   both).  */
static size_t iYlocal_packsize = IYPACKSIZE;
static size_t iYremote_packsize = IYPACKSIZE;

/* Local and remote packet sequence numbers.  */
static unsigned short iYlocal_pktnum;
static unsigned short iYremote_pktnum;

/* The timeout.  */
static int cYtimeout = CYTIMEOUT;

/* Transmitter buffer.  */
static char *zYbuf;

/* Protocol parameters.  */

struct uuconf_cmdtab asYproto_params[] =
{
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cYtimeout, NULL },
  { "packet-size", UUCONF_CMDTABTYPE_INT, (pointer) &iYlocal_packsize, NULL },
  { NULL, 0, NULL, NULL }
};

/* Local functions.  */

static boolean fywait_for_packet P((struct sdaemon *qdaemon,
				    boolean *pfexit));
static unsigned short iychecksum P((const char *z, size_t c));
static unsigned short iychecksum2 P((const char *zfirst, size_t cfirst,
				     const char *zsecond, size_t csecond));
static boolean fywait_for_header P((struct sdaemon *qdaemon,
				    unsigned short header[3], int timeout));
static boolean fysend_pkt P((struct sdaemon *qdaemon,
			     const void *zdata, size_t cdata));
static boolean fysend_control P((struct sdaemon *qdaemon,
				 int itype));
static boolean fyread_data P((struct sdaemon *qdaemon, size_t clen,
			      int timeout));

/* Exchange sync packets at protocol startup.  */

static boolean
fyxchg_syncs (qdaemon)
     struct sdaemon *qdaemon;
{
  char inithdr[Y_INIT_HDR_LEN];
  unsigned short header[3];
  unsigned short ichk;
  size_t clen, cfirst;
  int rpktsize;

  /* Send our configuration.  We could use only one array (for local
     and remote).  But this is safer in case the code changes and
     depend on separate ones.  */

  inithdr[Y_INIT_HDR_VERSION_OFF] = Y_VERSION;
  inithdr[Y_INIT_HDR_PKTSIZE_OFF] = iYlocal_packsize >> 8;
  TOLITTLE (inithdr + Y_INIT_HDR_FLAGS_OFF, 0);

  if (! fysend_pkt (qdaemon, inithdr, Y_INIT_HDR_LEN))
    return FALSE;

  if (! fywait_for_header (qdaemon, header, cYtimeout))
    return FALSE;

  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fyxchg_syncs: Got sync header");
  clen = header[YFRAME_LEN];

  if (clen < MIN_Y_SYNC || clen > MAX_Y_SYNC)
    {
      ulog (LOG_ERROR, "Bad 'y' protocol sync packet length");
      return FALSE;
    }

  /* It may be better to integrate this code with fywait_for_packet.  */
  if (! fyread_data (qdaemon, clen, cYtimeout))
    return FALSE;

  cfirst = CRECBUFLEN - iPrecstart;
  ichk = iychecksum2 (abPrecbuf + iPrecstart, cfirst,
		      abPrecbuf, clen - cfirst);

  rpktsize = BUCHAR (abPrecbuf[(iPrecstart + 1) % CRECBUFLEN]);

  /* Future versions of the protocol may need to check and react
     according to the version number.  */

  if (rpktsize == 0 || header[YFRAME_CHK] != ichk)
    {
      ulog (LOG_ERROR, "Bad 'y' protocol sync packet");
      return FALSE;
    }

  iYremote_packsize = rpktsize << 8;

  /* Some may want to do this different and in effect the protocol
     support this.  But I like the idea that the packet size would be
     the same in both directions.  This allows the caller to select
     both packet sizes without changing the configuration at the
     server.  */
  if (iYremote_packsize > iYlocal_packsize)
    iYremote_packsize = iYlocal_packsize;

  iPrecstart = (iPrecstart + clen) % CRECBUFLEN;
  return TRUE;
}

/* Start the protocol.  */

boolean
fystart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  *pzlog = NULL;

  /* This should force, or at least enable if available, RTS/CTS
     hardware flow control !! */

  /* The 'y' protocol requires an eight bit clean link */
  if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
		   STRIPSETTING_EIGHTBITS, XONXOFF_OFF))
    return FALSE;

  iYlocal_pktnum = iYremote_pktnum = 0;

  /* Only multiple of 256 sizes are allowed */
  iYlocal_packsize &= ~0xff;
  if (iYlocal_packsize < 256 || iYlocal_packsize > (16*1024))
    iYlocal_packsize = IYPACKSIZE;

  /* Exhange SYNC packets */
  if (! fyxchg_syncs (qdaemon))
    {
      /* Restore defaults */
      cYtimeout = CYTIMEOUT;
      iYlocal_packsize = IYPACKSIZE;
      return FALSE;
    }

  zYbuf = (char *) xmalloc (CYBUFSIZE + CYFRAMELEN);
  return TRUE;
}

/* Shutdown the protocol.  */

boolean 
fyshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  xfree ((pointer) zYbuf);
  zYbuf = NULL;
  cYtimeout = CYTIMEOUT;
  iYlocal_packsize = IYPACKSIZE;
  return TRUE;
}

/* Send a command string.  We send packets containing the string until
   the entire string has been sent, including the zero terminator.  */

/*ARGSUSED*/
boolean
fysendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  size_t clen;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fysendcmd: Sending command \"%s\"", z);

  clen = strlen (z) + 1;

  while (clen > 0)
    {
      size_t csize;

      csize = clen;
      if (csize > iYremote_packsize)
	csize = iYremote_packsize;

      if (! fysend_pkt (qdaemon, z, csize))
	return FALSE;

      z += csize;
      clen -= csize;
    }

  return TRUE;
}

/* Get space to be filled with data.  We always use zYbuf, which was
   allocated from the heap.  */

char *
zygetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = iYremote_packsize;
  return zYbuf + CYFRAMELEN;
}

/* Send out a data packet.  */

boolean
fysenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
#if DEBUG > 0
  if (cdata > iYremote_packsize)
    ulog (LOG_FATAL, "fysend_packet: Packet size too large");
#endif

  TOLITTLE (zYbuf + YFRAME_SEQ_OFF, iYlocal_pktnum);
  ++iYlocal_pktnum;
  TOLITTLE (zYbuf + YFRAME_LEN_OFF, cdata);
  TOLITTLE (zYbuf + YFRAME_CHK_OFF, iychecksum (zdata, cdata));

  /* We pass FALSE to fsend_data since we don't expect the other side
     to be sending us anything just now.  */
  return fsend_data (qdaemon->qconn, zYbuf, cdata + CYFRAMELEN, FALSE);
}

/* Wait for data to come in and process it until we've finished a
   command or a file.  */

boolean
fywait (qdaemon)
     struct sdaemon *qdaemon;
{
  boolean fexit = FALSE;

  while (! fexit)
    {
      if (! fywait_for_packet (qdaemon, &fexit))
	return FALSE;
    }
  return TRUE;
}

/* File level routines
   We could handle this inside the other public routines,
   but this is cleaner and better for future expansions */

boolean
fyfile (qdaemon, qtrans, fstart, fsend, cbytes, pfhandled)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
     boolean fstart;
     boolean fsend;
     long cbytes;
     boolean *pfhandled;
{
  unsigned short header[3];

  *pfhandled = FALSE;

  if (! fstart)
    {
      if (fsend)
	{
	  /* It is critical that the timeout here would be long
	     enough.  We have just sent a full file without any kind
	     of flow control at the protocol level.  The traffic may
	     be buffered in many places of the link, and the remote
	     may take a while until cathing up.  */
	  if (! fywait_for_header (qdaemon, header, cYtimeout * 2))
	    return FALSE;

	  if (header[YFRAME_CTL] != (unsigned short) YPKT_ACK)
	    {
	      DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fyfile: Error from remote: 0x%04X", header[1]);
	      ulog (LOG_ERROR, "Received 'y' protocol error from remote");
	      return FALSE;
	    }
	}
      else
	{
	  /* This is technically not requireed.  But I've put this in
	     the protocol to allow easier expansions.  */
	  return fysend_control (qdaemon, YPKT_ACK);
	}
    }

  return TRUE;
}

/* Send a control packet, not used during the normal file
   transmission.  */

static boolean
fysend_control (qdaemon, itype)
     struct sdaemon *qdaemon;
     int itype;
{
  char header[CYFRAMELEN];

  TOLITTLE (header + YFRAME_SEQ_OFF, iYlocal_pktnum);
  iYlocal_pktnum++;
  TOLITTLE (header + YFRAME_CTL_OFF, itype);
  TOLITTLE (header + YFRAME_CHK_OFF, 0);

  return fsend_data (qdaemon->qconn, header, CYFRAMELEN, FALSE);
}

/* Private function to send a packet.  This one doesn't need the data
   to be in the buffer provided by zygetspace.  I've found it worth
   for avoiding memory copies.  Somebody may want to do it otherwise */

static boolean
fysend_pkt (qdaemon, zdata, cdata)
     struct sdaemon *qdaemon;
     const void *zdata;
     size_t cdata;
{
  char header[CYFRAMELEN];

  TOLITTLE (header + YFRAME_SEQ_OFF, iYlocal_pktnum);
  iYlocal_pktnum++;
  TOLITTLE (header + YFRAME_LEN_OFF, cdata);
  TOLITTLE (header + YFRAME_CHK_OFF, iychecksum (zdata, cdata));

  if (! fsend_data (qdaemon->qconn, header, CYFRAMELEN, FALSE))
    return FALSE;
  return fsend_data (qdaemon->qconn, zdata, cdata, FALSE);
}

/* Wait until enough data arrived from the comm line.  This protocol
   doesn't need to perform any kind of action while waiting.  */

static boolean
fyread_data (qdaemon, clen, timeout)
     struct sdaemon *qdaemon;
     size_t clen;
     int timeout;
{
  int cinbuf;
  size_t crec;

  cinbuf = iPrecend - iPrecstart;
  if (cinbuf < 0)
    cinbuf += CRECBUFLEN;

  if (cinbuf < clen)
    {
      if (! freceive_data (qdaemon->qconn, clen - cinbuf, &crec,
			   timeout, TRUE))
	return FALSE;
      cinbuf += crec;
      if (cinbuf < clen)
	{
	  if (! freceive_data (qdaemon->qconn, clen - cinbuf, &crec,
			       timeout, TRUE))
	    return FALSE;
	}
      cinbuf += crec;
      if (cinbuf < clen)
	{
	  ulog (LOG_ERROR, "Timed out waiting for data");
	  return FALSE;
	}
    }

  return TRUE;
}

/* Receive a remote packet header, check for correct sequence number.  */

static boolean
fywait_for_header (qdaemon, header, timeout)
     struct sdaemon *qdaemon;
     unsigned short header[3];
     int timeout;
{
  if (! fyread_data (qdaemon, CYFRAMELEN, timeout))
    return FALSE;

  /* Somebody may want to optimize this in a portable way.  I'm not
     sure it's worth, but the output by gcc for the portable construct
     is so bad (even with optimization), that I couldn't resist.  */

  if (iPrecstart <= (CRECBUFLEN - CYFRAMELEN))
    {
      header[0] = FROMLITTLE (abPrecbuf + iPrecstart);
      header[1] = FROMLITTLE (abPrecbuf + iPrecstart + 2);
      header[2] = FROMLITTLE (abPrecbuf + iPrecstart + 4);
    }
  else
    {
      register int i, j;

      for (i = j = 0; j < CYFRAMELEN; i++, j += 2)
	{
	  header[i] =
	    (((abPrecbuf[(iPrecstart + j + 1) % CRECBUFLEN] & 0xff) << 8)
	     + (abPrecbuf[(iPrecstart + j) % CRECBUFLEN] & 0xff));
	}
    }

  iPrecstart = (iPrecstart + CYFRAMELEN) % CRECBUFLEN;

  DEBUG_MESSAGE3 (DEBUG_UUCP_PROTO,
		  "fywait_for_header: Got header: 0x%04X, 0x%04X, 0x%04X",
		  header[0], header[1], header[2]);

  if (header[YFRAME_SEQ] != iYremote_pktnum++)
    {
      ulog (LOG_ERROR, "Incorrect 'y' packet sequence");
      fysend_control (qdaemon, YPKT_BAD);
      return FALSE;
    }

  return TRUE;
}

/* Receive a remote data packet */

static boolean
fywait_for_packet (qdaemon, pfexit)
     struct sdaemon *qdaemon;
     boolean *pfexit;
{
  unsigned short header[3], ichk;
  size_t clen, cfirst;

  if (! fywait_for_header (qdaemon, header, cYtimeout))
    return FALSE;

  clen = header[YFRAME_LEN];
  if (clen == 0 && pfexit != NULL)
    {
      /* I Suppose the pointers could be NULL ??? */
      return fgot_data (qdaemon, abPrecbuf, 0, abPrecbuf, 0,
			-1, -1, (long) -1, TRUE, pfexit);
    }

  if (clen & 0x8000)
    {
      DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
		      "fywait_for_packet: Error from remote: 0x%04X",
		      header[YFRAME_CTL]);
      ulog (LOG_ERROR, "Remote error packet");
      return FALSE;
    }

  /* This is really not neccessary.  But if this check is removed,
     take in mind that the packet may be up to 32k long.  */
  if (clen > iYlocal_packsize)
    {
      ulog (LOG_ERROR, "Packet too large");
      return FALSE;
    }

  if (! fyread_data (qdaemon, clen, cYtimeout))
    return FALSE;

  cfirst = CRECBUFLEN - iPrecstart;
  if (cfirst > clen)
    cfirst = clen;

  if (cfirst == clen)
    ichk = iychecksum (abPrecbuf + iPrecstart, clen);
  else
    ichk = iychecksum2 (abPrecbuf + iPrecstart, cfirst,
			abPrecbuf, clen - cfirst);
  if (header[YFRAME_CHK] != ichk)
    {
      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
		      "fywait_for_packet: Bad checksum 0x%x != 0x%x",
		      header[YFRAME_CHK], ichk);
      fysend_control (qdaemon, YPKT_ERR);
      ulog (LOG_ERROR, "Checksum error");
      return FALSE;
    }

  if (pfexit != NULL
      && ! fgot_data (qdaemon, abPrecbuf + iPrecstart, cfirst, 
		      abPrecbuf, clen - cfirst,
		      -1, -1, (long) -1, TRUE, pfexit))
    return FALSE;

  iPrecstart = (iPrecstart + clen) % CRECBUFLEN;

  return TRUE;
}

/* Compute 16 bit checksum */

#ifdef __GNUC__
#ifdef __i386__
#define I386_ASM
#endif
#endif

#ifdef I386_ASM
#define ROTATE(i) \
	asm ("rolw $1,%0" : "=g" (i) : "g" (i))
#else
#define ROTATE(i) i += i + ((i & 0x8000) >> 15)
#endif

static unsigned short
iychecksum (z, c)
     register const char *z;
     register size_t c;
{
  register unsigned short ichk;

  ichk = 0xffff;

  while (c-- > 0)
    {
      ROTATE (ichk);
      ichk += BUCHAR (*z++);
    }

  return ichk;
}

static unsigned short
iychecksum2 (zfirst, cfirst, zsecond, csecond)
	const char *zfirst;
	size_t cfirst;
	const char *zsecond;
	size_t csecond;
{
  register unsigned short ichk;
  register const char *z;
  register size_t c;

  z = zfirst;
  c = cfirst + csecond;

  ichk = 0xffff;

  while (c-- > 0)
    {
      ROTATE (ichk);
      ichk += BUCHAR (*z++);

      /* If the first buffer has been finished, switch to the second.  */
      if (--cfirst == 0)
	z = zsecond;
    }

  return ichk;
}
