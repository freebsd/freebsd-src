/* proti.c
   The 'i' protocol.

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
const char proti_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucico/proti.c,v 1.7 1999/08/27 23:33:48 peter Exp $";
#endif

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* The 'i' protocol is a simple sliding window protocol, created by
   me.  It is in many ways similar to the 'g' protocol.  Several ideas
   are also taken from the paper ``A High-Throughput Message Transport
   System'' by P. Lauder.  I don't know where the paper was published,
   but the author's e-mail address is piers@cs.su.oz.au.  However, I
   haven't adopted his main idea, which is to dispense with windows
   entirely.  This is because some links still do require flow control
   and, more importantly, because I want to have a limit to the amount
   of data I must be able to resend upon request.  To reduce the costs
   of window acknowledgements, I use a large window and only require
   an ack at the halfway point.

   Each packet starts with a header containing the following
   information:

   Intro byte           8 bits          byte 1
   Packet number        5 bits          byte 2
   Local channel        3 bits
   Packet ack           5 bits          byte 3
   Remote channel       3 bits
   Packet type          3 bits          bytes 4-5
   Direction            1 bit
   Data length         12 bits
   Header check         8 bits          byte 6

   If the data length is not 0, this is followed by the data and a 32
   bit CRC checksum.

   The following packet types are defined:

   SYNC  Initialize the connection
   DATA  Data packet
   ACK   Simple acknowledgement with no data
   NAK   Negative acknowledgement; requests resend of single packet
   SPOS  Set file position
   CLOSE Close the connection
   */

/* The offsets of the bytes in the packet header.  */

#define IHDR_INTRO (0)
#define IHDR_LOCAL (1)
#define IHDR_REMOTE (2)
#define IHDR_CONTENTS1 (3)
#define IHDR_CONTENTS2 (4)
#define IHDR_CHECK (5)

/* Macros to set and extract values of IHDR_LOCAL and IHDR_REMOTE.  */
#define IHDRWIN_SET(iseq, ichan) (((iseq) << 3) | (ichan))
#define IHDRWIN_GETSEQ(ival) (((ival) >> 3) & 0x1f)
#define IHDRWIN_GETCHAN(ival) ((ival) & 0x07)

/* Macros to set and extract values of IHDR_CONTENTS fields.  */
#define IHDRCON_SET1(ttype, fcaller, cbytes) \
  (((ttype) << 5) | ((fcaller) ? (1 << 4) : 0) | (((cbytes) >> 8) & 0x0f))
#define IHDRCON_SET2(ttype, fcaller, cbytes) ((cbytes) & 0xff)
#define THDRCON_GETTYPE(i1, i2) (((i1) >> 5) & 0x07)
#define FHDRCON_GETCALLER(i1, i2) (((i1) & (1 << 4)) != 0)
#define CHDRCON_GETBYTES(i1, i2) ((((i1) & 0x0f) << 8) | ((i2) & 0xff))

/* Macros for the IHDR_CHECK field.  */
#define IHDRCHECK_VAL(zhdr) \
  ((zhdr[IHDR_LOCAL] \
    ^ zhdr[IHDR_REMOTE] \
    ^ zhdr[IHDR_CONTENTS1] \
    ^ zhdr[IHDR_CONTENTS2]) \
   & 0xff)

/* Length of the packet header.  */
#define CHDRLEN (6)

/* Amount of space to skip between start of packet and actual data.
   This is used to make the actual data longword aligned, to encourage
   good performance when copying data into the buffer.  */
#define CHDRSKIPLEN (CHDRLEN + (sizeof (long) - CHDRLEN % sizeof (long)))

/* Amount of space to skip between memory buffer and header.  */
#define CHDROFFSET (CHDRSKIPLEN - CHDRLEN)

/* Length of the trailing checksum.  */
#define CCKSUMLEN (4)

/* Macros to set and get the checksum.  These multiply evaluate their
   arguments.  */
#define ICKSUM_GET(z) \
  ((((((((unsigned long) ((z)[0] & 0xff)) << 8) \
       | (unsigned long) ((z)[1] & 0xff)) << 8) \
     | (unsigned long) ((z)[2] & 0xff)) << 8) \
   | (unsigned long) ((z)[3] & 0xff))
#define UCKSUM_SET(z, i) \
  (void) ((z)[0] = (((i) >> 24) & 0xff), \
	  (z)[1] = (((i) >> 16) & 0xff), \
	  (z)[2] = (((i) >> 8) & 0xff), \
	  (z)[3] = ((i) & 0xff))

/* The header introduction character.  */
#define IINTRO ('\007')

/* The packet types.  */

#define DATA (0)
#define SYNC (1)
#define ACK (2)
#define NAK (3)
#define SPOS (4)
#define CLOSE (5)

/* Largest possible packet size.  */
#define IMAXPACKSIZE ((1 << 12) - 1)

/* Largest possible sequence number (plus 1).  */
#define IMAXSEQ 32

/* Get the next sequence number given a sequence number.  */
#define INEXTSEQ(i) (((i) + 1) & (IMAXSEQ - 1))

/* Get the previous sequence number given a sequence number.  */
#define IPREVSEQ(i) (((i) + IMAXSEQ - 1) & (IMAXSEQ - 1))

/* Compute i1 - i2 in sequence space (i.e., the number of packets from
   i2 to i1).  */
#define CSEQDIFF(i1, i2) (((i1) + IMAXSEQ - (i2)) & (IMAXSEQ - 1))

/* Largest possible channel number (plus 1).  */
#define IMAXICHAN (8)

/* Default packet size to request (protocol parameter
   ``packet-size'').  */
#define IREQUEST_PACKSIZE (1024)

/* Default window size to request (protocol parameter ``window'').  */
#define IREQUEST_WINSIZE (16)

/* Default timeout to use when sending the SYNC packet (protocol
   parameter ``sync-timeout'').  */
#define CSYNC_TIMEOUT (10)

/* Default number of times to retry sending the SYNC packet (protocol
   parameter ``sync-retries'').  */
#define CSYNC_RETRIES (6)

/* Default timeout to use when waiting for a packet (protocol
   parameter ``timeout'').  */
#define CTIMEOUT (10)

/* Default number of times to retry sending a packet before giving up
   (protocol parameter ``retries'').  */
#define CRETRIES (6)

/* Default maximum level of errors to accept before giving up
   (protocol parameter ``errors'').  */
#define CERRORS (100)

/* Default decay rate.  Each time we receive this many packets
   succesfully, we decrement the error level by one (protocol
   parameter ``error-decay'').  */
#define CERROR_DECAY (10)

/* The default list of characters to avoid: XON and XOFF.  This string
   is processed as an escape sequence.  This is 'j' protocol parameter
   ``avoid''; it is defined in this file because the 'i' and 'j'
   protocols share protocol parameters.  */
#define ZAVOID "\\021\\023"

/* Local variables.  */

/* Packet size to request (protocol parameter ``packet-size'').  */
static int iIrequest_packsize = IREQUEST_PACKSIZE;

/* Window size to request (protocol parameter ``window'').  */
static int iIrequest_winsize = IREQUEST_WINSIZE;

/* Remote packet size (set from SYNC packet or from
   iIforced_remote_packsize).  */
static int iIremote_packsize;

/* Size which buffers were allocated for.  */
static int iIalc_packsize;

/* Forced remote packet size, used if non-zero (protocol parameter
   ``remote-packet-size'').  There is no forced remote window size
   because the ACK strategy requires that both sides agree on the
   window size.  */
static int iIforced_remote_packsize = 0;

/* Remote window size (set from SYNC packet).  */
static int iIremote_winsize;

/* Timeout to use when sending the SYNC packet (protocol
   parameter ``sync-timeout'').  */
int cIsync_timeout = CSYNC_TIMEOUT;

/* Number of times to retry sending the SYNC packet (protocol
   parameter ``sync-retries'').  */
static int cIsync_retries = CSYNC_RETRIES;

/* Timeout to use when waiting for a packet (protocol parameter
   ``timeout'').  */
static int cItimeout = CTIMEOUT;

/* Timeout to use when waiting for an acknowledgement to open up space
   in the window.  This is computed based on the window size and the
   connection speed.  */
static int cIwindow_timeout = CTIMEOUT;

/* Number of times to retry sending a packet before giving up
   (protocol parameter ``retries'').  */
static int cIretries = CRETRIES;

/* Maximum level of errors to accept before giving up (protocol
   parameter ``errors'').  */
static int cIerrors = CERRORS;

/* Each time we receive this many packets succesfully, we decrement
   the error level by one (protocol parameter ``error-decay'').  */
static int cIerror_decay = CERROR_DECAY;

/* The number of packets we should wait to receive before sending an
   ACK; this is set by default to half the window size we have
   requested (protocol parameter ``ack-frequency'').  */
static int cIack_frequency = 0;

/* The set of characters to avoid (protocol parameter ``avoid'').
   This is actually part of the 'j' protocol; it is defined in this
   file because the 'i' and 'j' protocols use the same protocol
   parameters.  */
const char *zJavoid_parameter = ZAVOID;

/* Routine to use when sending data.  This is a hook for the 'j'
   protocol.  */
static boolean (*pfIsend) P((struct sconnection *qconn, const char *zsend,
			     size_t csend, boolean fdoread));

/* Routine to use to use when reading data.  This is a hook for the
   'j' protocol.  */
static boolean (*pfIreceive) P((struct sconnection *qconn, size_t cneed,
				size_t *pcrec, int ctimeout,
				boolean freport));

/* Next sequence number to send.  */
static int iIsendseq;

/* Last sequence number received.  */
static int iIrecseq;

/* Last sequence number we have acknowledged.  */
static int iIlocal_ack;

/* Last sequence number remote system has acknowledged.  */
static int iIremote_ack;

/* File position we are sending from.  */
static long iIsendpos;

/* File position we are receiving to.  */
static long iIrecpos;

/* TRUE if closing the connection.  */
static boolean fIclosing;

/* Array of sent packets indexed by packet number.  */
static char *azIsendbuffers[IMAXSEQ];

/* Array of received packets that we aren't ready to process yet,
   indexed by packet number.  */
static char *azIrecbuffers[IMAXSEQ];

/* For each packet sequence number, record whether we sent a NAK for
   the packet.  */
static boolean afInaked[IMAXSEQ];

/* Number of SYNC packets received (used only to detect whether one
   was received).  */
static int cIsyncs;

/* Number of packets sent.  */
static long cIsent_packets;

/* Number of packets received.  */
static long cIreceived_packets;

/* Number of packets resent.  */
static long cIresent_packets;

/* Number of bad packet headers received.  */
static long cIbad_hdr;

/* Number of out of order packets received.  */
static long cIbad_order;

/* Number of bad checksums received.  */
static long cIbad_cksum;

/* Number of packets rejected by remote system.  */
static long cIremote_rejects;

/* Protocol parameter commands.  */

struct uuconf_cmdtab asIproto_params[] =
{
  { "packet-size", UUCONF_CMDTABTYPE_INT, (pointer) &iIrequest_packsize,
      NULL },
  { "window", UUCONF_CMDTABTYPE_INT, (pointer) &iIrequest_winsize, NULL },
  { "remote-packet-size", UUCONF_CMDTABTYPE_INT,
      (pointer) &iIforced_remote_packsize, NULL },
  { "sync-timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cIsync_timeout,
      NULL },
  { "sync-retries", UUCONF_CMDTABTYPE_INT, (pointer) &cIsync_retries,
      NULL },
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cItimeout, NULL },
  { "retries", UUCONF_CMDTABTYPE_INT, (pointer) &cIretries, NULL },
  { "errors", UUCONF_CMDTABTYPE_INT, (pointer) &cIerrors, NULL },
  { "error-decay", UUCONF_CMDTABTYPE_INT, (pointer) &cIerror_decay, NULL },
  { "ack-frequency", UUCONF_CMDTABTYPE_INT, (pointer) &cIack_frequency, NULL },
  /* The ``avoid'' protocol parameter is part of the 'j' protocol, but
     it is convenient for the 'i' and 'j' protocols to share the same
     protocol parameter table.  */
  { "avoid", UUCONF_CMDTABTYPE_STRING, (pointer) &zJavoid_parameter, NULL },
  { NULL, 0, NULL, NULL }
};

/* Local functions.  */

static boolean finak P((struct sdaemon *qdaemon, int iseq));
static boolean firesend P((struct sdaemon *qdaemon));
static boolean fiwindow_wait P((struct sdaemon *qdaemon));
static boolean fiwait_for_packet P((struct sdaemon *qdaemon,
				    int ctimeout, int cretries,
				    boolean fone, boolean *ftimedout));
static boolean ficheck_errors P((struct sdaemon *qdaemon));
static boolean fiprocess_data P((struct sdaemon *qdaemon,
				 boolean *pfexit, boolean *pffound,
				 size_t *pcneed));
static boolean fiprocess_packet P((struct sdaemon *qdaemon,
				   const char *zhdr,
				   const char *zfirst, int cfirst,
				   const char *zsecond, int csecond,
				   boolean *pfexit));

/* The 'i' protocol start routine.  The work is done in a routine
   which is also called by the 'j' protocol start routine.  */

boolean
fistart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  return fijstart (qdaemon, pzlog, IMAXPACKSIZE, fsend_data, freceive_data);
}

/* Start the protocol.  This routine is called by both the 'i' and 'j'
   protocol start routines.  We keep transmitting a SYNC packet
   containing the window and packet size we would like to receive
   until we receive a SYNC packet from the remote system.  The first
   two bytes of the data contents of a SYNC packet are the maximum
   packet size we want to receive (high byte, low byte), and the next
   byte is the maximum window size we want to use.  */

boolean
fijstart (qdaemon, pzlog, imaxpacksize, pfsend, pfreceive)
     struct sdaemon *qdaemon;
     char **pzlog;
     int imaxpacksize;
     boolean (*pfsend) P((struct sconnection *qconn, const char *zsend,
			  size_t csend, boolean fdoread));
     boolean (*pfreceive) P((struct sconnection *qconn, size_t cneed,
			     size_t *pcrec, int ctimeout, boolean freport));
{
  char ab[CHDRLEN + 4 + CCKSUMLEN];
  unsigned long icksum;
  int ctries;
  int csyncs;
  long ibaud;

  *pzlog = NULL;

  pfIsend = pfsend;
  pfIreceive = pfreceive;

  if (iIforced_remote_packsize <= 0
      || iIforced_remote_packsize > imaxpacksize)
    iIforced_remote_packsize = 0;
  else
    iIremote_packsize = iIforced_remote_packsize;
  iIalc_packsize = 0;

  iIsendseq = 1;
  iIrecseq = 0;
  iIlocal_ack = 0;
  iIremote_ack = 0;
  iIsendpos = 0;
  iIrecpos = 0;
  fIclosing = FALSE;

  cIsent_packets = 0;
  cIreceived_packets = 0;
  cIresent_packets = 0;
  cIbad_hdr = 0;
  cIbad_order = 0;
  cIbad_cksum = 0;
  cIremote_rejects = 0;

  if (iIrequest_packsize < 0 || iIrequest_packsize > imaxpacksize)
    {
      ulog (LOG_ERROR, "Illegal protocol '%c' packet size; using %d",
	    qdaemon->qproto->bname, imaxpacksize);
      iIrequest_packsize = imaxpacksize;
    }

  /* The maximum permissible window size is 16.  Otherwise the
     protocol can get confused because a duplicated packet may arrive
     out of order.  If the window size is large in such a case, the
     duplicate packet may be treated as a packet in the upcoming
     window, causing the protocol to assume that all intermediate
     packets have been lost, leading to immense confusion.  */
  if (iIrequest_winsize < 0 || iIrequest_winsize > IMAXSEQ / 2)
    {
      ulog (LOG_ERROR, "Illegal protocol '%c' window size; using %d",
	    qdaemon->qproto->bname, IREQUEST_WINSIZE);
      iIrequest_winsize = IREQUEST_WINSIZE;
    }

  /* The default for the ACK frequency is half the window size.  */
  if (cIack_frequency <= 0 || cIack_frequency >= iIrequest_winsize)
    cIack_frequency = iIrequest_winsize / 2;

  ab[IHDR_INTRO] = IINTRO;
  ab[IHDR_LOCAL] = ab[IHDR_REMOTE] = IHDRWIN_SET (0, 0);
  ab[IHDR_CONTENTS1] = IHDRCON_SET1 (SYNC, qdaemon->fcaller, 4);
  ab[IHDR_CONTENTS2] = IHDRCON_SET2 (SYNC, qdaemon->fcaller, 4);
  ab[IHDR_CHECK] = IHDRCHECK_VAL (ab);
  ab[CHDRLEN + 0] = (iIrequest_packsize >> 8) & 0xff;
  ab[CHDRLEN + 1] = iIrequest_packsize & 0xff;
  ab[CHDRLEN + 2] = iIrequest_winsize;
  ab[CHDRLEN + 3] = qdaemon->cchans;
  icksum = icrc (ab + CHDRLEN, 4, ICRCINIT);
  UCKSUM_SET (ab + CHDRLEN + 4, icksum);

  /* The static cIsyncs is incremented each time a SYNC packet is
     received.  */
  csyncs = cIsyncs;
  ctries = 0;

  while (TRUE)
    {
      boolean ftimedout;

      DEBUG_MESSAGE3 (DEBUG_PROTO,
		      "fistart: Sending SYNC packsize %d winsize %d channels %d",
		      iIrequest_packsize, iIrequest_winsize, qdaemon->cchans);

      if (! (*pfIsend) (qdaemon->qconn, ab, CHDRLEN + 4 + CCKSUMLEN,
			TRUE))
	return FALSE;

      if (fiwait_for_packet (qdaemon, cIsync_timeout, 0, FALSE,
			     &ftimedout))
	{
	  if (csyncs != cIsyncs)
	    break;
	}
      else
	{
	  if (! ftimedout)
	    return FALSE;

	  ++ctries;
	  if (ctries > cIsync_retries)
	    {
	      ulog (LOG_ERROR, "Protocol startup failed");
	      return FALSE;
	    }
	}
    }

  /* Calculate the window timeout.  */
  ibaud = iconn_baud (qdaemon->qconn);
  if (ibaud == 0)
    cIwindow_timeout = cItimeout;
  else
    {
      /* We expect to receive an ACK about halfway through each
	 window.  In principle, an entire window might be sitting in a
	 modem buffer while we are waiting for an ACK.  Therefore, the
	 minimum time we should wait for an ACK is
	   (1/2 window size) * (seconds / byte) + (roundtrip time) ==
	   (1/2 window size) * (1 / (baud / 10)) + (roundtrip time) ==
	   (1/2 window size) * (10 / baud) + (roundtrip time) ==
	   (5 * (window size)) / baud + (roundtrip time)

	 The window size is iIremote_packsize * iIremote_winsize.  For
	 typical settings of packsize == 1024, winsize == 16, baud ==
	 9600, this equation works out to
	   (5 * 1024 * 16) / 9600 == 8 seconds
	 We then take cItimeout as the round trip time, which gives us
	 some flexibility.  We get more flexibility because it is
	 quite likely that by the time we have finished sending out
	 the last packet in a window, the first one has already been
	 received by the remote system.  */
      cIwindow_timeout = ((5 * iIremote_packsize * iIremote_winsize) / ibaud
			  + cItimeout);
    }

  /* If we are the callee, bump both timeouts by one, to make it less
     likely that both systems will timeout simultaneously.  */
  if (! qdaemon->fcaller)
    {
      ++cItimeout;
      ++cIwindow_timeout;
    }

  /* We got a SYNC packet; set up packet buffers to use.  */
  if (iIremote_packsize > imaxpacksize)
    iIremote_packsize = imaxpacksize;
  do
    {
      int iseq;

      for (iseq = 0; iseq < IMAXSEQ; iseq++)
	{
	  azIrecbuffers[iseq] = NULL;
	  afInaked[iseq] = FALSE;
	  azIsendbuffers[iseq] = (char *) malloc (iIremote_packsize
						  + CHDRSKIPLEN
						  + CCKSUMLEN);
	  if (azIsendbuffers[iseq] == NULL)
	    {
	      int ifree;

	      for (ifree = 0; ifree < iseq; ifree++)
		free ((pointer) azIsendbuffers[ifree]);
	      break;
	    }
	}

      if (iseq >= IMAXSEQ)
	{
	  *pzlog =
	    zbufalc (sizeof "protocol '' sending packet/window / receiving /"
		     + 64);
	  sprintf (*pzlog,
		   "protocol '%c' sending packet/window %d/%d receiving %d/%d",
		   qdaemon->qproto->bname, (int) iIremote_packsize,
		   (int) iIremote_winsize, (int) iIrequest_packsize,
		   (int) iIrequest_winsize);

	  iIalc_packsize = iIremote_packsize;

	  return TRUE;
	}

      iIremote_packsize >>= 1;
    }
  while (iIremote_packsize > 200);

  ulog (LOG_ERROR,
	"'%c' protocol startup failed; insufficient memory for packets",
	qdaemon->qproto->bname);

  return FALSE;
}

/* Shut down the protocol.  We can be fairly informal about this,
   since we know that the upper level protocol has already exchanged
   hangup messages.  If we didn't know that, we would have to make
   sure that all packets before the CLOSE had been received.  */

boolean
fishutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  char *z;
  size_t clen;

  fIclosing = TRUE;

  z = zigetspace (qdaemon, &clen) - CHDRLEN;

  z[IHDR_INTRO] = IINTRO;
  z[IHDR_LOCAL] = IHDRWIN_SET (iIsendseq, 0);
  z[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
  iIlocal_ack = iIrecseq;
  z[IHDR_CONTENTS1] = IHDRCON_SET1 (CLOSE, qdaemon->fcaller, 0);
  z[IHDR_CONTENTS2] = IHDRCON_SET2 (CLOSE, qdaemon->fcaller, 0);
  z[IHDR_CHECK] = IHDRCHECK_VAL (z);

  DEBUG_MESSAGE0 (DEBUG_PROTO, "fishutdown: Sending CLOSE");

  if (! (*pfIsend) (qdaemon->qconn, z, CHDRLEN, FALSE))
    return FALSE;

  ulog (LOG_NORMAL,
	"Protocol '%c' packets: sent %ld, resent %ld, received %ld",
	qdaemon->qproto->bname, cIsent_packets, cIresent_packets,
	cIreceived_packets);
  if (cIbad_hdr != 0
      || cIbad_cksum != 0
      || cIbad_order != 0
      || cIremote_rejects != 0)
    ulog (LOG_NORMAL,
	  "Errors: header %ld, checksum %ld, order %ld, remote rejects %ld",
	  cIbad_hdr, cIbad_cksum, cIbad_order, cIremote_rejects);

  /* Reset the protocol parameters to their default values.  */
  iIrequest_packsize = IREQUEST_PACKSIZE;
  iIrequest_winsize = IREQUEST_WINSIZE;
  iIforced_remote_packsize = 0;
  cIsync_timeout = CSYNC_TIMEOUT;
  cIsync_retries = CSYNC_RETRIES;
  cItimeout = CTIMEOUT;
  cIwindow_timeout = CTIMEOUT;
  cIretries = CRETRIES;
  cIerrors = CERRORS;
  cIerror_decay = CERROR_DECAY;
  cIack_frequency = 0;
  zJavoid_parameter = ZAVOID;

  return TRUE;
}

/* Send a command string.  These are just sent as normal packets,
   ending in a packet containing a null byte.  */

boolean
fisendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  size_t clen;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fisendcmd: Sending command \"%s\"", z);

  clen = strlen (z);

  while (TRUE)
    {
      char *zpacket;
      size_t csize;

      zpacket = zigetspace (qdaemon, &csize);

      if (clen < csize)
	{
	  memcpy (zpacket, z, clen + 1);
	  return fisenddata (qdaemon, zpacket, clen + 1, ilocal, iremote,
			     (long) -1);
	}

      memcpy (zpacket, z, csize);
      z += csize;
      clen -= csize;

      if (! fisenddata (qdaemon, zpacket, csize, ilocal, iremote, (long) -1))
	return FALSE;
    }
  /*NOTREACHED*/
}

/* Send a NAK.  */

static boolean
finak (qdaemon, iseq)
     struct sdaemon *qdaemon;
     int iseq;
{
  char abnak[CHDRLEN];

  abnak[IHDR_INTRO] = IINTRO;
  abnak[IHDR_LOCAL] = IHDRWIN_SET (iseq, 0);
  abnak[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
  iIlocal_ack = iIrecseq;
  abnak[IHDR_CONTENTS1] = IHDRCON_SET1 (NAK, qdaemon->fcaller, 0);
  abnak[IHDR_CONTENTS2] = IHDRCON_SET2 (NAK, qdaemon->fcaller, 0);
  abnak[IHDR_CHECK] = IHDRCHECK_VAL (abnak);

  afInaked[iseq] = TRUE;

  DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
		  "finak: Sending NAK %d", iseq);

  return (*pfIsend) (qdaemon->qconn, abnak, CHDRLEN, TRUE);
}

/* Resend the latest packet the remote has not acknowledged.  */

static boolean
firesend (qdaemon)
     struct sdaemon *qdaemon;
{
  int iseq;
  char *zhdr;
  size_t clen;

  iseq = INEXTSEQ (iIremote_ack);
  if (iseq == iIsendseq)
    {
      /* Everything has been ACKed and there is nothing to resend.  */
      return TRUE;
    }

  DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
		  "firesend: Resending packet %d", iseq);

  /* Update the received sequence number.  */
  zhdr = azIsendbuffers[iseq] + CHDROFFSET;
  if (IHDRWIN_GETSEQ (zhdr[IHDR_REMOTE]) != iIrecseq)
    {
      int iremote;

      iremote = IHDRWIN_GETCHAN (zhdr[IHDR_REMOTE]);
      zhdr[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, iremote);
      zhdr[IHDR_CHECK] = IHDRCHECK_VAL (zhdr);
      iIlocal_ack = iIrecseq;
    }

  ++cIresent_packets;

  clen = CHDRCON_GETBYTES (zhdr[IHDR_CONTENTS1],
			   zhdr[IHDR_CONTENTS2]);

  return (*pfIsend) (qdaemon->qconn, zhdr,
		     CHDRLEN + clen + (clen > 0 ? CCKSUMLEN : 0),
		     TRUE);
}

/* Wait until there is an opening in the receive window of the remote
   system.  */

static boolean
fiwindow_wait (qdaemon)
     struct sdaemon *qdaemon;
{
  /* iIsendseq is the sequence number we are sending, and iIremote_ack
     is the last sequence number acknowledged by the remote. */
  while (CSEQDIFF (iIsendseq, iIremote_ack) > iIremote_winsize)
    {
      /* If a NAK is lost, it is possible for the other side to be
	 sending a stream of packets while we are waiting for an ACK.
	 This should be caught in fiprocess_data; if it is about to
	 send an ACK, but it has an unacknowledged packet to send, it
	 sends the entire packet.  Hopefully that will trigger an ACK
	 or a NAK and get us going again.  */
      DEBUG_MESSAGE0 (DEBUG_PROTO, "fiwindow_wait: Waiting for ACK");
      if (! fiwait_for_packet (qdaemon, cIwindow_timeout, cIretries,
			       TRUE, (boolean *) NULL))
	return FALSE;
    }

  return TRUE;
}

/* Get buffer space to use for packet data.  We return a pointer to
   the space to be used for the actual data, leaving room for the
   header.  */

/*ARGSUSED*/
char *
zigetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = iIremote_packsize;
  return azIsendbuffers[iIsendseq] + CHDRSKIPLEN;
}

/* Send a data packet.  The zdata argument will always point to value
   returned by zigetspace, so we know that we have room before it for
   the header information.  */

boolean
fisenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
  char *zhdr;
  unsigned long icksum;
  boolean fret;

#if DEBUG > 0
  if (ilocal < 0 || ilocal >= IMAXICHAN
      || iremote < 0 || iremote >= IMAXICHAN)
    ulog (LOG_FATAL, "fisenddata: ilocal %d iremote %d", ilocal, iremote);
#endif

  /* If we are changing the file position, we must send an SPOS
     packet.  */
  if (ipos != iIsendpos && ipos != (long) -1)
    {
      int inext;
      char *zspos;

      /* We need to get a buffer to hold the SPOS packet, and it needs
	 to be next sequence number.  However, the data we have been
	 given is currently in the next sequence number buffer.  So we
	 shuffle the buffers around.  */
      inext = INEXTSEQ (iIsendseq);
      zspos = azIsendbuffers[inext];
      azIsendbuffers[inext] = zdata - CHDRSKIPLEN;
      azIsendbuffers[iIsendseq] = zspos;
      zspos += CHDROFFSET;

      zspos[IHDR_INTRO] = IINTRO;
      zspos[IHDR_LOCAL] = IHDRWIN_SET (iIsendseq, 0);
      zspos[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
      iIlocal_ack = iIrecseq;
      zspos[IHDR_CONTENTS1] = IHDRCON_SET1 (SPOS, qdaemon->fcaller,
					    CCKSUMLEN);
      zspos[IHDR_CONTENTS2] = IHDRCON_SET2 (SPOS, qdaemon->fcaller,
					    CCKSUMLEN);
      zspos[IHDR_CHECK] = IHDRCHECK_VAL (zspos);
      UCKSUM_SET (zspos + CHDRLEN, (unsigned long) ipos);
      icksum = icrc (zspos + CHDRLEN, CCKSUMLEN, ICRCINIT);
      UCKSUM_SET (zspos + CHDRLEN + CCKSUMLEN, icksum);

      /* Wait for an opening in the window.  */
      if (iIremote_winsize > 0
	  && CSEQDIFF (iIsendseq, iIremote_ack) > iIremote_winsize)
	{
	  if (! fiwindow_wait (qdaemon))
	    return FALSE;
	}

      DEBUG_MESSAGE1 (DEBUG_PROTO, "fisenddata: Sending SPOS %ld",
		      ipos);

      if (! (*pfIsend) (qdaemon->qconn, zspos,
			CHDRLEN + CCKSUMLEN + CCKSUMLEN, TRUE))
	return FALSE;

      iIsendseq = INEXTSEQ (iIsendseq);
      iIsendpos = ipos;
    }

  zhdr = zdata - CHDRLEN;
  zhdr[IHDR_INTRO] = IINTRO;
  zhdr[IHDR_LOCAL] = IHDRWIN_SET (iIsendseq, ilocal);
  zhdr[IHDR_CONTENTS1] = IHDRCON_SET1 (DATA, qdaemon->fcaller, cdata);
  zhdr[IHDR_CONTENTS2] = IHDRCON_SET2 (DATA, qdaemon->fcaller, cdata);

  /* Compute and set the checksum.  */
  if (cdata > 0)
    {
      icksum = icrc (zdata, cdata, ICRCINIT);
      UCKSUM_SET (zdata + cdata, icksum);
    }

  /* Wait until there is an opening in the window (we hope to not have
     to wait here at all, actually; ideally the window should be large
     enough to avoid a wait).  */
  if (iIremote_winsize > 0
      && CSEQDIFF (iIsendseq, iIremote_ack) > iIremote_winsize)
    {
      if (! fiwindow_wait (qdaemon))
	return FALSE;
    }

  /* We only fill in IHDR_REMOTE now, since only now do know the
     correct value of iIrecseq.  */
  zhdr[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, iremote);
  iIlocal_ack = iIrecseq;
  zhdr[IHDR_CHECK] = IHDRCHECK_VAL (zhdr);

  DEBUG_MESSAGE4 (DEBUG_PROTO,
		  "fisenddata: Sending packet %d size %d local %d remote %d",
		  iIsendseq, (int) cdata, ilocal, iremote);		  

  iIsendseq = INEXTSEQ (iIsendseq);
  ++cIsent_packets;

  fret = (*pfIsend) (qdaemon->qconn, zhdr,
		     cdata + CHDRLEN + (cdata > 0 ? CCKSUMLEN : 0),
		     TRUE);

  iIsendpos += cdata;

  if (fret && iPrecstart != iPrecend)
    {
      boolean fexit;

      fret = fiprocess_data (qdaemon, &fexit, (boolean *) NULL,
			     (size_t *) NULL);
    }

  return fret;
}

/* Wait for data to come in.  */

boolean
fiwait (qdaemon)
     struct sdaemon *qdaemon;
{
  return fiwait_for_packet (qdaemon, cItimeout, cIretries,
			    FALSE, (boolean *) NULL);
}

/* Wait for a packet.  Either there is no data to send, or the remote
   window is full.  */

static boolean
fiwait_for_packet (qdaemon, ctimeout, cretries, fone, pftimedout)
     struct sdaemon *qdaemon;
     int ctimeout;
     int cretries;
     boolean fone;
     boolean *pftimedout;
{
  int cshort;
  int ctimeouts;

  if (pftimedout != NULL)
    *pftimedout = FALSE;

  cshort = 0;
  ctimeouts = 0;

  while (TRUE)
    {
      boolean fexit, ffound;
      size_t cneed;
      size_t crec;

      if (! fiprocess_data (qdaemon, &fexit, &ffound, &cneed))
	return FALSE;

      if (fexit || (fone && ffound))
	return TRUE;

      if (cneed == 0)
	continue;

      DEBUG_MESSAGE1 (DEBUG_PROTO, "fiwait_for_packet: Need %d bytes",
		      (int) cneed);

      if (! (*pfIreceive) (qdaemon->qconn, cneed, &crec, ctimeout, TRUE))
	return FALSE;

      if (crec != 0)
	{
	  /* If we didn't get enough data twice in a row, we may have
	     dropped some data and be waiting for the end of a large
	     packet.  Incrementing iPrecstart will force
	     fiprocess_data to skip the current packet and try to find
	     the next one.  */
	  if (crec >= cneed)
	    cshort = 0;
	  else
	    {
	      ++cshort;
	      if (cshort > 1)
		{
		  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
		  cshort = 0;
		}
	    }
	}
      else
	{
	  int i;

	  /* We timed out on the read.  */
	  ++ctimeouts;
	  if (ctimeouts > cretries)
	    {
	      if (cretries > 0)
		ulog (LOG_ERROR, "Timed out waiting for packet");
	      if (pftimedout != NULL)
		*pftimedout = TRUE;
	      return FALSE;
	    }

	  /* Clear out the list of packets we have sent NAKs for.  We
	     should have seen some sort of response by now.  */
	  for (i = 0; i < IMAXSEQ; i++)
	    afInaked[i] = FALSE;

	  /* Send a NAK for the packet we want, and, if we have an
	     unacknowledged packet, send it again.  */
	  if (! finak (qdaemon, INEXTSEQ (iIrecseq))
	      || ! firesend (qdaemon))
	    return FALSE;
	}
    }
  /*NOTREACHED*/
}

/* Make sure we haven't overflowed the permissible error level.  */

static boolean
ficheck_errors (qdaemon)
     struct sdaemon *qdaemon;
{
  if (cIerrors < 0)
    return TRUE;

  if (((cIbad_order + cIbad_hdr + cIbad_cksum + cIremote_rejects)
       - (cIreceived_packets / cIerror_decay))
      > cIerrors)
    {
      /* Try shrinking the packet size.  */
      if (iIrequest_packsize > 400)
	{
	  char absync[CHDRLEN + 3 + CCKSUMLEN];
	  unsigned long icksum;

	  /* Don't bother sending the number of channels in this
	     packet.  */
	  iIrequest_packsize /= 2;
	  absync[IHDR_INTRO] = IINTRO;
	  absync[IHDR_LOCAL] = IHDRWIN_SET (0, 0);
	  absync[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
	  iIlocal_ack = iIrecseq;
	  absync[IHDR_CONTENTS1] = IHDRCON_SET1 (SYNC, qdaemon->fcaller, 3);
	  absync[IHDR_CONTENTS2] = IHDRCON_SET2 (SYNC, qdaemon->fcaller, 3);
	  absync[IHDR_CHECK] = IHDRCHECK_VAL (absync);
	  absync[CHDRLEN + 0] = (iIrequest_packsize >> 8) & 0xff;
	  absync[CHDRLEN + 1] = iIrequest_packsize & 0xff;
	  absync[CHDRLEN + 2] = iIrequest_winsize;
	  icksum = icrc (absync + CHDRLEN, 3, ICRCINIT);
	  UCKSUM_SET (absync + CHDRLEN + 3, icksum);

	  cIerrors *= 2;

	  DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			  "ficheck_errors: Sending SYNC packsize %d winsize %d",
			  iIrequest_packsize, iIrequest_winsize);

	  return (*pfIsend) (qdaemon->qconn, absync,
			     CHDRLEN + 3 + CCKSUMLEN, TRUE);
	}

      ulog (LOG_ERROR, "Too many '%c' protocol errors",
	    qdaemon->qproto->bname);
      return FALSE;
    }

  return TRUE;
}

/* Process data waiting in the receive buffer, passing to the
   fgot_data function.  */

static boolean
fiprocess_data (qdaemon, pfexit, pffound, pcneed)
     struct sdaemon *qdaemon;
     boolean *pfexit;
     boolean *pffound;
     size_t *pcneed;
{
  boolean fbadhdr;

  if (pfexit != NULL)
    *pfexit = FALSE;
  if (pffound != NULL)
    *pffound = FALSE;

  fbadhdr = FALSE;

  while (iPrecstart != iPrecend)
    {
      char ab[CHDRLEN];
      int cfirst, csecond;
      char *zfirst, *zsecond;
      int i;
      int iget;
      int ttype;
      int iseq;
      int csize;
      int iack;

      /* If we're closing the connection, ignore any data remaining in
	 the input buffer.  */
      if (fIclosing)
	{
	  if (pfexit != NULL)
	    *pfexit = TRUE;
	  if (pcneed != NULL)
	    *pcneed = 0;
	  return TRUE;
	}

      /* Look for the IINTRO character.  */
      if (abPrecbuf[iPrecstart] != IINTRO)
	{
	  char *zintro;
	  int cintro;

	  cintro = iPrecend - iPrecstart;
	  if (cintro < 0)
	    cintro = CRECBUFLEN - iPrecstart;

	  zintro = memchr (abPrecbuf + iPrecstart, IINTRO, (size_t) cintro);

	  if (zintro == NULL)
	    {
	      iPrecstart = (iPrecstart + cintro) % CRECBUFLEN;
	      continue;
	    }

	  /* We don't need % CRECBUFLEN here because zintro - (abPrecbuf
	     + iPrecstart) < cintro <= CRECBUFLEN - iPrecstart.  */
	  iPrecstart += zintro - (abPrecbuf + iPrecstart);
	}

      /* Get the header into ab.  */
      for (i = 0, iget = iPrecstart;
	   i < CHDRLEN && iget != iPrecend;
	   i++, iget = (iget + 1) % CRECBUFLEN)
	ab[i] = abPrecbuf[iget];

      if (i < CHDRLEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CHDRLEN - i;
	  return TRUE;
	}

      if ((ab[IHDR_CHECK] & 0xff) != IHDRCHECK_VAL (ab)
	  || (FHDRCON_GETCALLER (ab[IHDR_CONTENTS1], ab[IHDR_CONTENTS2])
	      ? qdaemon->fcaller : ! qdaemon->fcaller))
	{
	  /* We only report a single bad header message per call, to
	     avoid generating many errors if we get many INTRO bytes
	     in a row.  */
	  if (! fbadhdr)
	    {
	      DEBUG_MESSAGE0 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fiprocess_data: Bad header");

	      ++cIbad_hdr;
	      if (! ficheck_errors (qdaemon))
		return FALSE;

	      fbadhdr = TRUE;
	    }

	  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	  continue;
	}

      zfirst = zsecond = NULL;
      cfirst = csecond = 0;

      ttype = THDRCON_GETTYPE (ab[IHDR_CONTENTS1], ab[IHDR_CONTENTS2]);
      if (ttype == DATA || ttype == SPOS || ttype == CLOSE)
	iseq = IHDRWIN_GETSEQ (ab[IHDR_LOCAL]);
      else
	iseq = -1;
      csize = CHDRCON_GETBYTES (ab[IHDR_CONTENTS1], ab[IHDR_CONTENTS2]);

      if (iseq != -1)
	{
	  /* Make sure this packet is in our receive window.  The last
	     packet we have acked is iIlocal_ack.  */
	  if (iIrequest_winsize > 0
	      && CSEQDIFF (iseq, iIlocal_ack) > iIrequest_winsize)
	    {
	      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fiprocess_data: Out of order packet %d (ack %d)",
			      iseq, iIlocal_ack);

	      ++cIbad_order;
	      if (! ficheck_errors (qdaemon))
		return FALSE;

	      iPrecstart = (iPrecstart + 1) % CRECBUFLEN;

	      continue;
	    }
	}

      if (csize > 0)
	{
	  int cinbuf;
	  char abcksum[CCKSUMLEN];
	  unsigned long ickdata;

	  cinbuf = iPrecend - iPrecstart;
	  if (cinbuf < 0)
	    cinbuf += CRECBUFLEN;
	  if (cinbuf < CHDRLEN + csize + CCKSUMLEN)
	    {
	      if (pcneed != NULL)
		*pcneed = CHDRLEN + csize + CCKSUMLEN - cinbuf;
	      return TRUE;
	    }

	  if (iPrecend > iPrecstart)
	    {
	      cfirst = csize;
	      zfirst = abPrecbuf + iPrecstart + CHDRLEN;
	    }
	  else
	    {
	      cfirst = CRECBUFLEN - (iPrecstart + CHDRLEN);
	      if (cfirst <= 0)
		{
		  /* Here cfirst is non-positive, so subtracting from
		     abPrecbuf will actually skip the appropriate number
		     of bytes at the start of abPrecbuf.  */
		  zfirst = abPrecbuf - cfirst;
		  cfirst = csize;
		}
	      else
		{
		  if (cfirst >= csize)
		    cfirst = csize;
		  else
		    {
		      zsecond = abPrecbuf;
		      csecond = csize - cfirst;
		    }
		  zfirst = abPrecbuf + iPrecstart + CHDRLEN;
		}
	    }

	  /* Get the checksum into abcksum.  */
	  for (i = 0, iget = (iPrecstart + CHDRLEN + csize) % CRECBUFLEN;
	       i < CCKSUMLEN;
	       i++, iget = (iget + 1) % CRECBUFLEN)
	    abcksum[i] = abPrecbuf[iget];

	  ickdata = icrc (zfirst, (size_t) cfirst, ICRCINIT);
	  if (csecond > 0)
	    ickdata = icrc (zsecond, (size_t) csecond, ickdata);

	  if (ICKSUM_GET (abcksum) != ickdata)
	    {
	      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fiprocess_data: Bad checksum; data %lu, frame %lu",
			      ickdata, ICKSUM_GET (abcksum));

	      ++cIbad_cksum;
	      if (! ficheck_errors (qdaemon))
		return FALSE;

	      /* If this sequence number is in our receive window,
		 send a NAK.  iIrecseq is the last sequence number we
		 have succesfully received.  */
	      if (iseq != -1
		  && iseq != iIrecseq
		  && (iIrequest_winsize <= 0
		      || CSEQDIFF (iseq, iIrecseq) <= iIrequest_winsize)
		  && azIrecbuffers[iseq] == NULL)
		{
		  if (! finak (qdaemon, iseq))
		    return FALSE;
		}

	      iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	      continue;
	    }
	}

      /* Here we know that this is a valid packet, so we can adjust
	 iPrecstart accordingly.  */
      if (csize == 0)
	iPrecstart = (iPrecstart + CHDRLEN) % CRECBUFLEN;
      else
	{
	  iPrecstart = ((iPrecstart + CHDRLEN + csize + CCKSUMLEN)
			% CRECBUFLEN);
	  ++cIreceived_packets;
	}

      /* Get the ack from the packet, if appropriate.  iIsendseq is
	 the next sequence number we are going to send, and
	 iIremote_ack is the last sequence number acknowledged by the
	 remote system.  */
      iack = IHDRWIN_GETSEQ (ab[IHDR_REMOTE]);
      if (iIremote_winsize > 0
	  && iack != iIsendseq
	  && CSEQDIFF (iack, iIremote_ack) <= iIremote_winsize
	  && CSEQDIFF (iIsendseq, iack) <= iIremote_winsize)
	{
	  /* Call uwindow_acked each time packet 0 is acked.  */
	  if (iack < iIremote_ack)
	    uwindow_acked (qdaemon, FALSE);
	  iIremote_ack = iack;
	}

      if (iseq != -1)
	{
	  /* If we already sent a NAK for this packet, and we have not
	     seen the previous packet, then forget that we sent a NAK
	     for this and any preceding packets.  This is to handle
	     the following sequence:
	         receive packet 0
		 packets 1 and 2 lost
		 receive packet 3
		 send NAK 1
		 send NAK 2
		 packet 1 lost
		 receive packet 2
	     At this point we want to send NAK 1.  */
	  if (afInaked[iseq]
	      && azIrecbuffers[IPREVSEQ (iseq)] == NULL)
	    {
	      for (i = INEXTSEQ (iIrecseq);
		   i != iseq;
		   i = INEXTSEQ (i))
		afInaked[i] = FALSE;
	      afInaked[iseq] = FALSE;
	    }

	  /* If we haven't handled all previous packets, we must save
	     off this packet and deal with it later.  */
	  if (iseq != INEXTSEQ (iIrecseq))
	    {
	      if (iseq == iIrecseq
		  || (iIrequest_winsize > 0
		      && CSEQDIFF (iseq, iIrecseq) > iIrequest_winsize))
		{
		  DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
				  "fiprocess_data: Ignoring out of order packet %d (recseq %d)",
				  iseq, iIrecseq);
		  continue;
		}
	      else
		{
		  DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
				  "fiprocess_data: Saving unexpected packet %d (recseq %d)",
				  iseq, iIrecseq);

		  if (azIrecbuffers[iseq] == NULL)
		    {
		      azIrecbuffers[iseq] = zbufalc ((size_t) (CHDRLEN
							       + csize));
		      memcpy (azIrecbuffers[iseq], ab, CHDRLEN);
		      if (csize > 0)
			{
			  memcpy (azIrecbuffers[iseq] + CHDRLEN, zfirst,
				  (size_t) cfirst);
			  if (csecond > 0)
			    memcpy (azIrecbuffers[iseq] + CHDRLEN + cfirst,
				    zsecond, (size_t) csecond);
			}
		    }
		}

	      /* Send NAK's for each packet between the last one we
		 received and this one, avoiding any packets for which
		 we've already sent NAK's or which we've already
		 received.  */
	      for (i = INEXTSEQ (iIrecseq);
		   i != iseq;
		   i = INEXTSEQ (i))
		{
		  if (! afInaked[i]
		      && azIrecbuffers[i] == NULL)
		    {
		      if (! finak (qdaemon, i))
			return FALSE;
		    }
		}

	      continue;
	    }

	  iIrecseq = iseq;
	}

      if (pffound != NULL)
	*pffound = TRUE;

      if (! fiprocess_packet (qdaemon, ab, zfirst, cfirst, zsecond, csecond,
			      pfexit))
	return FALSE;

      if (iseq != -1)
	{
	  int inext;

	  /* If we've already received the next packet(s), process
	     them.  */
	  inext = INEXTSEQ (iIrecseq);
	  while (azIrecbuffers[inext] != NULL)
	    {
	      char *z;
	      int c;

	      z = azIrecbuffers[inext];
	      c = CHDRCON_GETBYTES (z[IHDR_CONTENTS1], z[IHDR_CONTENTS2]);
	      iIrecseq = inext;
	      if (! fiprocess_packet (qdaemon, z, z + CHDRLEN, c,
				      (char *) NULL, 0, pfexit))
		return FALSE;
	      ubuffree (azIrecbuffers[inext]);
	      azIrecbuffers[inext] = NULL;
	      inext = INEXTSEQ (inext);
	    }
	}

      /* If we have received half of our window size or more since the
	 last ACK, send one now.  Sending an ACK for half the window
	 at a time should significantly cut the acknowledgement
	 traffic when only one side is sending.  We should normally
	 not have to send an ACK if we have data to send, since each
	 packet sent will ACK the most recently received packet.
	 However, it can happen if we receive a burst of short
	 packets, such as a set of command acknowledgements.  */
      if (iIrequest_winsize > 0
	  && CSEQDIFF (iIrecseq, iIlocal_ack) >= cIack_frequency)
	{
	  char aback[CHDRLEN];

	  aback[IHDR_INTRO] = IINTRO;
	  aback[IHDR_LOCAL] = IHDRWIN_SET (0, 0);
	  aback[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
	  iIlocal_ack = iIrecseq;
	  aback[IHDR_CONTENTS1] = IHDRCON_SET1 (ACK, qdaemon->fcaller, 0);
	  aback[IHDR_CONTENTS2] = IHDRCON_SET2 (ACK, qdaemon->fcaller, 0);
	  aback[IHDR_CHECK] = IHDRCHECK_VAL (aback);

	  DEBUG_MESSAGE1 (DEBUG_PROTO, "fiprocess_data: Sending ACK %d",
			  iIrecseq);

	  if (! (*pfIsend) (qdaemon->qconn, aback, CHDRLEN, TRUE))
	    return FALSE;
	}
    }

  if (pcneed != NULL)
    *pcneed = CHDRLEN;

  return TRUE;
}

/* Process a single packet.  */

static boolean
fiprocess_packet (qdaemon, zhdr, zfirst, cfirst, zsecond, csecond, pfexit)
     struct sdaemon *qdaemon;
     const char *zhdr;
     const char *zfirst;
     int cfirst;
     const char *zsecond;
     int csecond;
     boolean *pfexit;
{
  int ttype;

  ttype = THDRCON_GETTYPE (zhdr[IHDR_CONTENTS1], zhdr[IHDR_CONTENTS2]);
  switch (ttype)
    {
    case DATA:
      {
	int iseq;
	boolean fret;

	iseq = IHDRWIN_GETSEQ (zhdr[IHDR_LOCAL]);
	DEBUG_MESSAGE4 (DEBUG_PROTO,
			"fiprocess_packet: Got DATA packet %d size %d local %d remote %d",
			iseq, cfirst + csecond,
			IHDRWIN_GETCHAN (zhdr[IHDR_REMOTE]),
			IHDRWIN_GETCHAN (zhdr[IHDR_LOCAL]));
	fret = fgot_data (qdaemon, zfirst, (size_t) cfirst,
			  zsecond, (size_t) csecond,
			  IHDRWIN_GETCHAN (zhdr[IHDR_REMOTE]),
			  IHDRWIN_GETCHAN (zhdr[IHDR_LOCAL]),
			  iIrecpos,
			  INEXTSEQ (iIremote_ack) == iIsendseq,
			  pfexit);
	iIrecpos += cfirst + csecond;
	return fret;
      }

    case SYNC:
      {
	int ipack, iwin, cchans;

	/* We accept a SYNC packet to adjust the packet and window
	   sizes at any time.  */
	if (cfirst + csecond < 3)
	  {
	    ulog (LOG_ERROR, "Bad SYNC packet");
	    return FALSE;
	  }
	ipack = (zfirst[0] & 0xff) << 8;
	if (cfirst > 1)
	  ipack |= zfirst[1] & 0xff;
	else
	  ipack |= zsecond[0];
	if (cfirst > 2)
	  iwin = zfirst[2];
	else
	  iwin = zsecond[2 - cfirst];

	/* The fourth byte in a SYNC packet is the number of channels
	   to use.  This is optional.  Switching the number of
	   channels in the middle of a conversation may cause
	   problems.  */
	if (cfirst + csecond <= 3)
	  cchans = 0;
	else
	  {
	    if (cfirst > 3)
	      cchans = zfirst[3];
	    else
	      cchans = zsecond[3 - cfirst];
	    if (cchans > 0 && cchans < 8)
	      qdaemon->cchans = cchans;
	  }

	DEBUG_MESSAGE3 (DEBUG_PROTO,
			"fiprocess_packet: Got SYNC packsize %d winsize %d channels %d",
			ipack, iwin, cchans);

	if (iIforced_remote_packsize == 0
	    && (iIalc_packsize == 0
		|| ipack <= iIalc_packsize))
	  iIremote_packsize = ipack;
	iIremote_winsize = iwin;

	/* We increment a static variable to tell the initialization
	   code that a SYNC was received, and we set *pfexit to TRUE
	   to get out to the initialization code (this will do no harm
	   if we are called from elsewhere).  */
	++cIsyncs;
	*pfexit = TRUE;
	return TRUE;
      }

    case ACK:
      /* There is nothing to do here, since the ack was already
	 handled in fiprocess_data.  */
      DEBUG_MESSAGE1 (DEBUG_PROTO,
		      "fiprocess_packet: Got ACK %d",
		      IHDRWIN_GETSEQ (zhdr[IHDR_REMOTE]));
      return TRUE;

    case NAK:
      /* We must resend the requested packet.  */
      {      
	int iseq;
	char *zsend;
	size_t clen;

	++cIremote_rejects;
	if (! ficheck_errors (qdaemon))
	  return FALSE;

	iseq = IHDRWIN_GETSEQ (zhdr[IHDR_LOCAL]);

	/* If the remote side times out while waiting for a packet, it
	   will send a NAK for the next packet it wants to see.  If we
	   have not sent that packet yet, and we have no
	   unacknowledged data, it implies that the remote side has a
	   window full of data to send, which implies that our ACK has
	   been lost.  Therefore, we send an ACK.  */
	if (iseq == iIsendseq &&
	    INEXTSEQ (iIremote_ack) == iIsendseq)
	  {
	    char aback[CHDRLEN];

	    aback[IHDR_INTRO] = IINTRO;
	    aback[IHDR_LOCAL] = IHDRWIN_SET (0, 0);
	    aback[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, 0);
	    iIlocal_ack = iIrecseq;
	    aback[IHDR_CONTENTS1] = IHDRCON_SET1 (ACK, qdaemon->fcaller, 0);
	    aback[IHDR_CONTENTS2] = IHDRCON_SET2 (ACK, qdaemon->fcaller, 0);
	    aback[IHDR_CHECK] = IHDRCHECK_VAL (aback);

	    DEBUG_MESSAGE1 (DEBUG_PROTO, "fiprocess_packet: Sending ACK %d",
			    iIrecseq);

	    return (*pfIsend) (qdaemon->qconn, aback, CHDRLEN, TRUE);
	  }
	else
	  {
	    if (iseq == iIsendseq
		|| (iIremote_winsize > 0
		    && (CSEQDIFF (iseq, iIremote_ack) > iIremote_winsize
			|| CSEQDIFF (iIsendseq, iseq) > iIremote_winsize)))
	      {
		DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
				"fiprocess_packet: Ignoring out of order NAK %d (sendseq %d)",
				iseq, iIsendseq);
		return TRUE;
	      }

	    DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
			    "fiprocess_packet: Got NAK %d; resending packet",
			    iseq);

	    /* Update the received sequence number.  */
	    zsend = azIsendbuffers[iseq] + CHDROFFSET;
	    if (IHDRWIN_GETSEQ (zsend[IHDR_REMOTE]) != iIrecseq)
	      {
		int iremote;

		iremote = IHDRWIN_GETCHAN (zsend[IHDR_REMOTE]);
		zsend[IHDR_REMOTE] = IHDRWIN_SET (iIrecseq, iremote);
		zsend[IHDR_CHECK] = IHDRCHECK_VAL (zsend);
		iIlocal_ack = iIrecseq;
	      }
	      
	    ++cIresent_packets;

	    clen = CHDRCON_GETBYTES (zsend[IHDR_CONTENTS1],
				     zsend[IHDR_CONTENTS2]);

	    return (*pfIsend) (qdaemon->qconn, zsend,
			       CHDRLEN + clen + (clen > 0 ? CCKSUMLEN : 0),
			       TRUE);
	  }
      }

    case SPOS:
      /* Set the file position.  */
      {
	char abpos[CCKSUMLEN];
	const char *zpos;

	if (cfirst >= CCKSUMLEN)
	  zpos = zfirst;
	else
	  {
	    memcpy (abpos, zfirst, (size_t) cfirst);
	    memcpy (abpos + cfirst, zsecond, (size_t) (CCKSUMLEN - cfirst));
	    zpos = abpos;
	  }
	iIrecpos = (long) ICKSUM_GET (zpos);
	DEBUG_MESSAGE1 (DEBUG_PROTO,
			"fiprocess_packet: Got SPOS %ld", iIrecpos);
	return TRUE;
      }

    case CLOSE:
      {
	boolean fexpected;

	fexpected = ! fLog_sighup || fIclosing;
	if (! fexpected)
	  ulog (LOG_ERROR, "Received unexpected CLOSE packet");
	else
	  DEBUG_MESSAGE0 (DEBUG_PROTO, "fiprocess_packet: Got CLOSE packet");

	fIclosing = TRUE;
	*pfexit = TRUE;
	return fexpected;
      }

    default:
      /* Just ignore unrecognized packet types, for future protocol
	 enhancements.  */
      DEBUG_MESSAGE1 (DEBUG_PROTO, "fiprocess_packet: Got packet type %d",
		      ttype);
      return TRUE;
    }
  /*NOTREACHED*/
}
