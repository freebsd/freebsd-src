/* protg.c
   The 'g' protocol.

   Copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor

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
const char protg_rcsid[] = "$Id: protg.c,v 1.2 1994/05/07 18:13:46 ache Exp $";
#endif

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* Each 'g' protocol packet begins with six bytes.  They are:

   <DLE><k><c0><c1><C><x>

   <DLE> is the ASCII DLE character (^P or '\020').
   if 1 <= <k> <= 8, the packet is followed by 2 ** (k + 4) bytes of data;
   if <k> == 9, these six bytes are a complete control packet;
   other value of <k> are illegal.
   <c0> is the low byte of a checksum.
   <c1> is the high byte of a checksum.
   <C> is a control byte (see below).
   <x> is <k> ^ <c0> ^ <c1> ^ <C>.

   The control byte <C> is divided into three bitfields:

   t t x x x y y y

   The two bit field tt is the packet type.
   The three bit field xxx is the control type for a control packet, or
   the sequence number for a data packet.
   The three bit field yyy is a value for a control packet, or the
   sequence number of the last packet received for a data packet.

   For all successfully recieved packets, the control byte is stored
   into iGpacket_control.  */

/* Names for the bytes in the frame header.  */
#define IFRAME_DLE (0)
#define IFRAME_K (1)
#define IFRAME_CHECKLOW (2)
#define IFRAME_CHECKHIGH (3)
#define IFRAME_CONTROL (4)
#define IFRAME_XOR (5)

/* Length of the frame header.  */
#define CFRAMELEN (6)

/* Macros to break apart the control bytes.  */
#define CONTROL_TT(b) ((int)(((b) >> 6) & 03))
#define CONTROL_XXX(b) ((int)(((b) >> 3) & 07))
#define CONTROL_YYY(b) ((int)((b) & 07))

/* DLE value.  */
#define DLE ('\020')

/* Get the length of a packet given a pointer to the header.  */
#define CPACKLEN(z) ((size_t) (1 << ((z)[IFRAME_K] + 4)))

/* <k> field value for a control message.  */
#define KCONTROL (9)

/* Get the next sequence number given a sequence number.  */
#define INEXTSEQ(i) ((i + 1) & 07)

/* Compute i1 - i2 modulo 8.  */
#define CSEQDIFF(i1, i2) (((i1) + 8 - (i2)) & 07)

/* Packet types.  These are from the tt field.
   CONTROL -- control packet
   ALTCHAN -- alternate channel; not used by UUCP
   DATA -- full data segment
   SHORTDATA -- less than full data segment (all the bytes specified by
   the packet length <k> are always transferred).  Let <u> be the number
   of bytes in the data segment not to be used.  If <u> <= 0x7f, the first
   byte of the data segment is <u> and the data follows.  If <u> > 0x7f,
   the first byte of the data segment is 0x80 | (<u> & 0x7f), the second
   byte of the data segment is <u> >> 7, and the data follows.  The
   maximum possible data segment size is 2**12, so this handles all
   possible cases.  */
#define CONTROL (0)
#define ALTCHAN (1)
#define DATA (2)
#define SHORTDATA (3)

/* Control types.  These are from the xxx field if the type (tt field)
   is CONTROL.

   CLOSE -- close the connection
   RJ -- reject; packet yyy last to be received correctly
   SRJ -- selective reject; reject only packet yyy (not used by UUCP)
   RR -- receiver ready; packet yyy received correctly
   INITC -- third step of initialization; yyy holds window size
   INITB -- second step of initialization; yyy holds maximum <k> value - 1
   INITA -- first step of initialization; yyy holds window size.

   The yyy value for RR is the same as the yyy value for an ordinary
   data packet.  */
#define CLOSE (1)
#define RJ (2)
#define SRJ (3)
#define RR (4)
#define INITC (5)
#define INITB (6)
#define INITA (7)

/* Maximum amount of data in a single packet.  This is set by the <k>
   field in the header; the amount of data in a packet is
   2 ** (<k> + 4).  <k> ranges from 1 to 8.  */
    
#define CMAXDATAINDEX (8)

#define CMAXDATA (1 << (CMAXDATAINDEX + 4))

/* Maximum window size.  */
#define CMAXWINDOW (7)

/* Defaults for the protocol parameters.  These may all be changed by
   using the ``protocol-parameter g'' command, so there is no
   particular reason to change the values given here.  */

/* The desired window size.  This is what we tell the other system to
   use.  It must be between 1 and 7, and there's no reason to use less
   than 7.  Protocol parameter ``window''.  */
#define IWINDOW (7)

/* The desired packet size.  Many implementations only support 64 byte
   packets.  Protocol parameter ``packet-size''.  */
#define IPACKSIZE (64)

/* The number of times to retry the exchange of INIT packets when
   starting the protocol.  Protocol parameter ``startup-retries''.  */
#define CSTARTUP_RETRIES (8)

/* The timeout to use when waiting for an INIT packet when starting up
   the protocol.  Protocol parameter ``init-timeout''.  */
#define CEXCHANGE_INIT_TIMEOUT (10)

/* The number of times to retry sending and waiting for a single INIT
   packet when starting the protocol.  This controls a single INIT
   packet, while CSTARTUP_RETRIES controls how many times to try the
   entire INIT sequence.  Protocol parameter ``init-retries''.  */
#define CEXCHANGE_INIT_RETRIES (4)

/* The timeout to use when waiting for a packet.  Protocol parameter
   ``timeout''.  */
#define CTIMEOUT (10)

/* The number of times to retry waiting for a packet.  Each time the
   timeout fails we send a copy of our last data packet or a reject
   message for the packet we expect from the other side, depending on
   whether we are waiting for an acknowledgement or a data packet.
   This is the number of times we try doing that and then waiting
   again.  Protocol parameter ``retries''.   */
#define CRETRIES (6)

/* If we see more than this much unrecognized data, we drop the
   connection.  This must be larger than a single packet size, which
   means it must be larger than 4096 (the largest possible packet
   size).  Protocol parameter ``garbage''.  */
#define CGARBAGE (10000)

/* If we see more than this many protocol errors, we drop the
   connection.  Protocol parameter ``errors''.  */
#define CERRORS (100)

/* Default decay rate.  Each time we send or receive this many packets
   succesfully, we decrement the error level by one (protocol
   parameter ``error-decay'').  */
#define CERROR_DECAY (10)

/* If this value is non-zero, it will be used as the remote window
   size regardless of what the other side requested.  This can be
   useful for dealing with some particularly flawed packages.  This
   default value should always be 0, and protocol parameter
   ``remote-window'' should be used for the affected systems.  */
#define IREMOTE_WINDOW (0)

/* If this value is non-zero, it will be used as the packet size to
   send to the remote system regardless of what it requested.  It's
   difficult to imagine any circumstances where you would want to set
   this.  Protocol parameter ``remote-packet-size''.  */
#define IREMOTE_PACKSIZE (0)

/* Local variables.  */

/* Next sequence number to send.  */
static int iGsendseq;

/* Last sequence number that has been acked.  */
static int iGremote_ack;

/* Last sequence number to be retransmitted.  */
static int iGretransmit_seq;

/* Last sequence number we have received.  */
static int iGrecseq;

/* Last sequence number we have acked.  */
static int iGlocal_ack;

/* Window size to request (protocol parameter ``window'').  */
static int iGrequest_winsize = IWINDOW;

/* Packet size to request (protocol parameter ``packet-size'').  */
static int iGrequest_packsize = IPACKSIZE;

/* Remote window size (set during handshake).  */
static int iGremote_winsize;

/* Forced remote window size (protocol parameter ``remote-window'').  */
static int iGforced_remote_winsize = IREMOTE_WINDOW;

/* Remote segment size (set during handshake).  This is one less than
   the value in a packet header.  */
static int iGremote_segsize;

/* Remote packet size (set based on iGremote_segsize).  */
static size_t iGremote_packsize;

/* Forced remote packet size (protocol parameter
   ``remote-packet-size'').  */
static int iGforced_remote_packsize = IREMOTE_PACKSIZE;

/* Recieved control byte.  */
static int iGpacket_control;

/* Number of times to retry the initial handshake.  Protocol parameter
   ``startup-retries''.  */
static int cGstartup_retries = CSTARTUP_RETRIES;

/* Number of times to retry sending an initial control packet.
   Protocol parameter ``init-retries''.  */
static int cGexchange_init_retries = CEXCHANGE_INIT_RETRIES;

/* Timeout (seconds) for receiving an initial control packet.
   Protocol parameter ``init-timeout''.  */
static int cGexchange_init_timeout = CEXCHANGE_INIT_TIMEOUT;

/* Timeout (seconds) for receiving a data packet.  Protocol parameter
   ``timeout''.  */
static int cGtimeout = CTIMEOUT;

/* Maximum number of timeouts when receiving a data packet or
   acknowledgement.  Protocol parameter ``retries''.  */
static int cGretries = CRETRIES;

/* Amount of garbage data we are prepared to see before giving up.
   Protocol parameter ``garbage''.  */
static int cGgarbage_data = CGARBAGE;

/* Maximum number of errors we are prepared to see before giving up.
   Protocol parameter ``errors''.  */
static int cGmax_errors = CERRORS;

/* Each time we receive this many packets succesfully, we decrement
   the error level by one (protocol parameter ``error-decay'').  */
static int cGerror_decay = CERROR_DECAY;

/* Whether to use shorter packets when possible.  Protocol parameter
   ``short-packets''.  */
static boolean fGshort_packets = TRUE;

/* Protocol parameter commands.  */
struct uuconf_cmdtab asGproto_params[] =
{
  { "window", UUCONF_CMDTABTYPE_INT, (pointer) &iGrequest_winsize, NULL },
  { "packet-size", UUCONF_CMDTABTYPE_INT, (pointer) &iGrequest_packsize,
      NULL },
  { "startup-retries", UUCONF_CMDTABTYPE_INT, (pointer) &cGstartup_retries,
      NULL },
  { "init-timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cGexchange_init_timeout,
      NULL },
  { "init-retries", UUCONF_CMDTABTYPE_INT, (pointer) &cGexchange_init_retries,
      NULL },
  { "timeout", UUCONF_CMDTABTYPE_INT, (pointer) &cGtimeout, NULL },
  { "retries", UUCONF_CMDTABTYPE_INT, (pointer) &cGretries, NULL },
  { "garbage", UUCONF_CMDTABTYPE_INT, (pointer) &cGgarbage_data, NULL },
  { "errors", UUCONF_CMDTABTYPE_INT, (pointer) &cGmax_errors, NULL },
  { "error-decay", UUCONF_CMDTABTYPE_INT, (pointer) &cGerror_decay, NULL },
  { "remote-window", UUCONF_CMDTABTYPE_INT,
      (pointer) &iGforced_remote_winsize, NULL },
  { "remote-packet-size", UUCONF_CMDTABTYPE_INT,
      (pointer) &iGforced_remote_packsize, NULL },
  { "short-packets", UUCONF_CMDTABTYPE_BOOLEAN, (pointer) &fGshort_packets,
      NULL },
  { NULL, 0, NULL, NULL }
};

/* Statistics.  */

/* Number of packets we have sent.  */
static long cGsent_packets;

/* Number of packets we have resent (these are not included in
   cGsent_packets).  */
static long cGresent_packets;

/* Number of packets we have delayed sending (these should not be
   counted in cGresent_packets).  */
static long cGdelayed_packets;

/* Number of packets we have received.  */
static long cGrec_packets;

/* Number of packets rejected because the header was bad.  */
static long cGbad_hdr;

/* Number of packets rejected because the checksum was bad.  */
static long cGbad_checksum;

/* Number of packets received out of order.  */
static long cGbad_order;

/* Number of packets rejected by receiver (number of RJ packets
   received).  */
static long cGremote_rejects;

/* Number of duplicate RR packets treated as RJ packets.  Some UUCP
   packages appear to never send RJ packets, but only RR packets.  If
   no RJ has been seen, fgprocess_data treats a duplicate RR as an RJ
   and increments this variable.  */
static long cGremote_duprrs;

/* The error level.  This is the total number of errors as adjusted by
   cGerror_decay.  */
static long cGerror_level;

/* Each time we send an RJ, we can expect several out of order of
   packets, because the other side will probably have sent a full
   window by the time it sees the RJ.  This variable keeps track of
   the number of out of order packets we expect to see.  We don't
   count expected out of order packets against the error level.  This
   is reset to 0 when an in order packet is received.  */
static int cGexpect_bad_order;

#if DEBUG > 1
/* Control packet names used for debugging.  */
static const char * const azGcontrol[] =
{"?0?", "CLOSE", "RJ", "SRJ", "RR", "INITC", "INITB", "INITA"};
#endif

/* Local functions.  */
static boolean fgexchange_init P((struct sdaemon *qdaemon, int ictl,
				  int ival, int *piset));
static boolean fgsend_control P((struct sdaemon *qdaemon, int ictl,
				 int ival));
static char *zgadjust_ack P((int iseq));
static boolean fgwait_for_packet P((struct sdaemon *qdaemon,
				    boolean freturncontrol, int ctimeout,
				    int cretries));
static boolean fgsend_acks P((struct sdaemon *qdaemon));
static boolean fggot_ack P((struct sdaemon *qdaemon, int iack));
static boolean fgprocess_data P((struct sdaemon *qdaemon, boolean fdoacks,
				 boolean freturncontrol,
				 boolean *pfexit, size_t *pcneed,
				 boolean *pffound));
static boolean fginit_sendbuffers P((boolean fallocate));
static boolean fgcheck_errors P((struct sdaemon *qdaemon));
static int igchecksum P((const char *zdata, size_t clen));
static int igchecksum2 P((const char *zfirst, size_t cfirst,
			  const char *zsecond, size_t csecond));

/* Start the protocol.  This requires a three way handshake.  Both sides
   must send and receive an INITA packet, an INITB packet, and an INITC
   packet.  The INITA and INITC packets contain the window size, and the
   INITB packet contains the packet size.  */

boolean
fgstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  int iseg;
  int i;
  boolean fgota, fgotb;

  *pzlog = NULL;

  /* The 'g' protocol requires a full eight bit interface.  */
  if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
		   STRIPSETTING_EIGHTBITS, XONXOFF_OFF))
    return FALSE;

  iGsendseq = 1;
  iGremote_ack = 0;
  iGretransmit_seq = -1;
  iGrecseq = 0;
  iGlocal_ack = 0;
  cGsent_packets = 0;
  cGresent_packets = 0;
  cGdelayed_packets = 0;
  cGrec_packets = 0;
  cGbad_hdr = 0;
  cGbad_checksum = 0;
  cGbad_order = 0;
  cGremote_rejects = 0;
  cGremote_duprrs = 0;
  cGerror_level = 0;
  cGexpect_bad_order = 0;

  /* We must determine the segment size based on the packet size
     which may have been modified by a protocol parameter command.
     A segment size of 2^n is passed as n - 5.  */
  i = iGrequest_packsize;
  iseg = -1;
  while (i > 0)
    {
      ++iseg;
      i >>= 1;
    }
  iseg -= 5;
  if (iseg < 0 || iseg > 7)
    {
      ulog (LOG_ERROR, "Illegal packet size %d for '%c' protocol",
	    iGrequest_packsize, qdaemon->qproto->bname);
      iseg = 1;
    }
  
  if (iGrequest_winsize <= 0 || iGrequest_winsize > 7)
    {
      ulog (LOG_ERROR, "Illegal window size %d for '%c' protocol",
	    iGrequest_winsize, qdaemon->qproto->bname);
      iGrequest_winsize = IWINDOW;
    }

  fgota = FALSE;
  fgotb = FALSE;
  for (i = 0; i < cGstartup_retries; i++)
    {
      if (fgota)
	{
	  if (! fgsend_control (qdaemon, INITA, iGrequest_winsize))
	    return FALSE;
	}
      else
	{
	  if (! fgexchange_init (qdaemon, INITA, iGrequest_winsize,
				 &iGremote_winsize))
	    continue;
	}
      fgota = TRUE;

      if (fgotb)
	{
	  if (! fgsend_control (qdaemon, INITB, iseg))
	    return FALSE;
	}
      else
	{
	  if (! fgexchange_init (qdaemon, INITB, iseg, &iGremote_segsize))
	    continue;
	}
      fgotb = TRUE;

      if (! fgexchange_init (qdaemon, INITC, iGrequest_winsize,
			     &iGremote_winsize))
	continue;

      /* We have succesfully connected.  Determine the remote packet
	 size.  */
      iGremote_packsize = 1 << (iGremote_segsize + 5);

      /* If the user requested us to force specific remote window and
	 packet sizes, do so now.  */
      if (iGforced_remote_winsize > 0
	  && iGforced_remote_winsize <= CMAXWINDOW)
	iGremote_winsize = iGforced_remote_winsize;

      if (iGforced_remote_packsize >= 32
	  && iGforced_remote_packsize <= 4096)
	{
	  /* Force the value to a power of two.  */
	  i = iGforced_remote_packsize;
	  iseg = -1;
	  while (i > 0)
	    {
	      ++iseg;
	      i >>= 1;
	    }
	  iGremote_packsize = 1 << iseg;
	  iGremote_segsize = iseg - 5;
	}

      /* Set up packet buffers to use.  We don't do this until we know
	 the maximum packet size we are going to send.  */
      if (! fginit_sendbuffers (TRUE))
	return FALSE;

      *pzlog =
	zbufalc (sizeof "protocol '' sending packet/window / receiving /"
		 + 64);
      sprintf (*pzlog,
	       "protocol '%c' sending packet/window %d/%d receiving %d/%d",
	       qdaemon->qproto->bname, (int) iGremote_packsize,
	       (int) iGremote_winsize, (int) iGrequest_packsize,
	       (int) iGrequest_winsize);

      return TRUE;
    }

  DEBUG_MESSAGE0 (DEBUG_PROTO | DEBUG_ABNORMAL,
		  "fgstart: Protocol startup failed");

  return FALSE;
}

/* The 'G' protocol is identical to the 'g' protocol, except that
   short packets are never supported.  */

boolean
fbiggstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  fGshort_packets = FALSE;
  return fgstart (qdaemon, pzlog);
}

/* The 'v' protocol is identical to the 'g' protocol, except that the
   packet size defaults to 512 bytes.  Rather than really get it
   right, we automatically switch from the usual default of 64 to 512.
   This won't work correctly if somebody does protocol-parameter v
   packet-size 64.  */

boolean
fvstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  if (iGrequest_packsize == IPACKSIZE)
    iGrequest_packsize = 1024;
  return fgstart (qdaemon, pzlog);
}  

/* Exchange initialization messages with the other system.

   A problem:

   We send INITA; it gets received
   We receive INITA
   We send INITB; it gets garbled
   We receive INITB

   We have seen and sent INITB, so we start to send INITC.  The other
   side as sent INITB but not seen it, so it times out and resends
   INITB.  We will continue sending INITC and the other side will
   continue sending INITB until both sides give up and start again
   with INITA.

   It might seem as though if we are sending INITC and receive INITB,
   we should resend our INITB, but this could cause infinite echoing
   of INITB on a long-latency line.  Rather than risk that, I have
   implemented a fast drop-back procedure.  If we are sending INITB and
   receive INITC, the other side has gotten ahead of us.  We immediately
   fail and begin again with INITA.  For the other side, if we are
   sending INITC and see INITA, we also immediately fail back to INITA.

   Unfortunately, this doesn't work for the other case, in which we
   are sending INITB but the other side has not yet seen INITA.  As
   far as I can see, if this happens we just have to wait until we
   time out and resend INITA.  */

static boolean
fgexchange_init (qdaemon, ictl, ival, piset)
     struct sdaemon *qdaemon;
     int ictl;
     int ival;
     int *piset;
{
  int i;

  /* The three-way handshake should be independent of who initializes
     it, but it seems that some versions of uucico assume that the
     caller sends first and the callee responds.  This only matters if
     we are the callee and the first packet is garbled.  If we send a
     packet, the other side will assume that we must have seen the
     packet they sent and will never time out and send it again.
     Therefore, if we are the callee we don't send a packet the first
     time through the loop.  This can still fail, but should usually
     work, and, after all, if the initialization packets are received
     correctly there will be no problem no matter what we do.  */
  for (i = 0; i < cGexchange_init_retries; i++)
    {
      long itime;
      int ctimeout;

      if (qdaemon->fcaller || i > 0)
	{
	  if (! fgsend_control (qdaemon, ictl, ival))
	    return FALSE;
	}

      itime = ixsysdep_time ((long *) NULL);
      ctimeout = cGexchange_init_timeout;

      do
	{
	  long inewtime;

	  /* We pass 0 as the retry count to fgwait_for_packet because
	     we want to handle retries here and because if it retried
	     it would send a packet, which would be bad.  */
	  if (! fgwait_for_packet (qdaemon, TRUE, ctimeout, 0))
	    break;

	  if (CONTROL_TT (iGpacket_control) == CONTROL)
	    {
	      if (CONTROL_XXX (iGpacket_control) == ictl)
		{
		  *piset = CONTROL_YYY (iGpacket_control);

		  /* If we didn't already send our initialization
		     packet, send it now.  */
		  if (! qdaemon->fcaller && i == 0)
		    {
		      if (! fgsend_control (qdaemon, ictl, ival))
			return FALSE;
		    }

		  return TRUE;
		}

	      /* If the other side is farther along than we are,
		 we have lost a packet.  Fail immediately back to
		 INITA (but don't fail if we are already doing INITA,
		 since that would count against cStart_retries more
		 than it should).  */
	      if (CONTROL_XXX (iGpacket_control) < ictl && ictl != INITA)
		return FALSE;

	      /* If we are sending INITC and we receive an INITA, the other
		 side has failed back (we know this because we have
		 seen an INITB from them).  Fail back ourselves to
		 start the whole handshake over again.  */
	      if (CONTROL_XXX (iGpacket_control) == INITA && ictl == INITC)
		return FALSE;

	      /* As a special hack, if we are sending INITC and we
		 receive INITB, we update the segment size from the
		 packet.  This permits a second INITB to override the
		 first one.  It would be nice to do this in a cleaner
		 way.  */
	      if (CONTROL_XXX (iGpacket_control) == INITB && ictl == INITC)
		iGremote_segsize = CONTROL_YYY (iGpacket_control);
	    }

	  inewtime = ixsysdep_time ((long *) NULL);
	  ctimeout -= inewtime - itime;
	}
      while (ctimeout > 0);
    }

  return FALSE;
}

/* Shut down the protocol.  */

boolean
fgshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  (void) fgsend_control (qdaemon, CLOSE, 0);
  (void) fgsend_control (qdaemon, CLOSE, 0);
  (void) fginit_sendbuffers (FALSE);

  /* The count of sent packets may not be accurate, because some of
     them may have not been sent yet if the connection failed in the
     middle (the ones that counted for cGdelayed_packets).  I don't
     think it's worth being precise.  */
  ulog (LOG_NORMAL,
	"Protocol '%c' packets: sent %ld, resent %ld, received %ld",
	qdaemon->qproto->bname, cGsent_packets,
	cGresent_packets - cGdelayed_packets, cGrec_packets);
  if (cGbad_hdr != 0
      || cGbad_checksum != 0
      || cGbad_order != 0
      || cGremote_rejects != 0
      || cGremote_duprrs != 0)
    ulog (LOG_NORMAL,
	  "Errors: header %ld, checksum %ld, order %ld, remote rejects %ld",
	  cGbad_hdr, cGbad_checksum, cGbad_order,
	  cGremote_duprrs + cGremote_rejects);

  /* Reset all the parameters to their default values, so that the
     protocol parameters used for this connection do not affect the
     next one.  */
  iGrequest_winsize = IWINDOW;
  iGrequest_packsize = IPACKSIZE;
  cGstartup_retries = CSTARTUP_RETRIES;
  cGexchange_init_timeout = CEXCHANGE_INIT_TIMEOUT;
  cGexchange_init_retries = CEXCHANGE_INIT_RETRIES;
  cGtimeout = CTIMEOUT;
  cGretries = CRETRIES;
  cGgarbage_data = CGARBAGE;
  cGmax_errors = CERRORS;
  cGerror_decay = CERROR_DECAY;
  iGforced_remote_winsize = IREMOTE_WINDOW;
  iGforced_remote_packsize = IREMOTE_PACKSIZE;
  fGshort_packets = TRUE;

  return TRUE;
}

/* Send a command string.  We send packets containing the string until
   the entire string has been sent.  Each packet is full.  */

/*ARGSUSED*/
boolean
fgsendcmd (qdaemon, z, ilocal, iremote)
     struct sdaemon *qdaemon;
     const char *z;
     int ilocal;
     int iremote;
{
  size_t clen;
  boolean fagain;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fgsendcmd: Sending command \"%s\"", z);

  clen = strlen (z);

  do
    {
      char *zpacket;
      size_t cdummy;

      zpacket = zggetspace (qdaemon, &cdummy);

      if (clen < iGremote_packsize)
	{
	  size_t csize;

	  /* If the remote packet size is larger than 64 (the default,
	     which may indicate an older UUCP package), try to fit
	     this command into a smaller packet.  We still always send
	     a complete packet, though.  */
	  if (iGremote_packsize <= 64 || ! fGshort_packets)
	    csize = iGremote_packsize;
	  else
	    {
	      csize = 32;
	      while (csize <= clen)
		csize <<= 1;
	    }

	  memcpy (zpacket, z, clen);
	  if (csize > clen)
	    bzero (zpacket + clen, csize - clen);
	  fagain = FALSE;

	  if (! fgsenddata (qdaemon, zpacket, csize, 0, 0, (long) 0))
	    return FALSE;
	}
      else
	{
	  memcpy (zpacket, z, iGremote_packsize);
	  z += iGremote_packsize;
	  clen -= iGremote_packsize;
	  fagain = TRUE;

	  if (! fgsenddata (qdaemon, zpacket, iGremote_packsize,
			    0, 0, (long) 0))
	    return FALSE;
	}
    }
  while (fagain);

  return TRUE;
}

/* We keep an array of buffers to retransmit as necessary.  Rather
   than waste static space on large buffer sizes, we allocate the
   buffers once we know how large the other system expects them to be.
   The sequence numbers used in the 'g' protocol are only three bits
   long, so we allocate eight buffers and maintain a correspondence
   between buffer index and sequence number.  This always wastes some
   buffer space, but it's easy to implement.

   We leave room at the front of the buffer for the frame header and
   two additional bytes.  The two extra bytes are used for short
   packets, which essentially use a longer header and shorter data.
   We do this to avoid moving the data.  We zero out any unused bytes
   before the frame, so we can locate the real header given a buffer
   by finding the first non-zero byte (which will be one of the first
   three bytes in the buffer).  */

#define CSENDBUFFERS (CMAXWINDOW + 1)

static char *azGsendbuffers[CSENDBUFFERS];

static boolean
fginit_sendbuffers (fallocate)
     boolean fallocate;
{
  int i;

  /* Free up any remaining old buffers.  */
  for (i = 0; i < CSENDBUFFERS; i++)
    {
      xfree ((pointer) azGsendbuffers[i]);
      if (fallocate)
	{
	  azGsendbuffers[i] = (char *) malloc (CFRAMELEN + 2
					       + iGremote_packsize);
	  if (azGsendbuffers[i] == NULL)
	    return FALSE;

	  /* This bzero might not seem necessary, since before we send
	     out each packet we zero out any non-data bytes.  However,
	     if we receive an SRJ at the start of the conversation, we
	     will send out the packet before it has been set to
	     anything, thus sending the contents of our heap.  We
	     avoid this by using bzero.  */
	  bzero (azGsendbuffers[i], CFRAMELEN + 2 + iGremote_packsize);
	}
      else
	azGsendbuffers[i] = NULL;
    }
  return TRUE;
}

/* Allocate a packet to send out.  The return value of this function
   must be filled in and passed to fgsenddata, or discarded.  This
   will ensure that the buffers and iGsendseq stay in synch.  Set
   *pclen to the amount of data to place in the buffer.  */

/*ARGSUSED*/
char *
zggetspace (qdaemon, pclen)
     struct sdaemon *qdaemon;
     size_t *pclen;
{
  *pclen = iGremote_packsize;
  return azGsendbuffers[iGsendseq] + CFRAMELEN + 2;
}

/* Send out a data packet.  This computes the checksum, sets up the
   header, and sends the packet out.  The argument zdata should point
   to the return value of zggetspace.  */

/*ARGSIGNORED*/
boolean
fgsenddata (qdaemon, zdata, cdata, ilocal, iremote, ipos)
     struct sdaemon *qdaemon;
     char *zdata;
     size_t cdata;
     int ilocal;
     int iremote;
     long ipos;
{
  char *z;
  int itt, iseg;
  size_t csize;
  int iclr1, iclr2;
  unsigned short icheck;

  /* Set the initial length bytes.  See the description at the definition
     of SHORTDATA, above.  */
  itt = DATA;
  csize = iGremote_packsize;
  iseg = iGremote_segsize + 1;

#if DEBUG > 0
  if (cdata > csize)
    ulog (LOG_FATAL, "fgsend_packet: Packet size too large");
#endif

  iclr1 = -1;
  iclr2 = -2;
  if (cdata < csize)
    {
      /* If the remote packet size is larger than 64, the default, we
	 can assume they can handle a smaller packet as well, which
	 will be more efficient to send.  */
      if (iGremote_packsize > 64 && fGshort_packets)
	{
	  /* The packet size is 1 << (iseg + 4).  */
	  iseg = 1;
	  csize = 32;
	  while (csize < cdata)
	    {
	      csize <<= 1;
	      ++iseg;
	    }
	}

      if (csize != cdata)
	{
	  size_t cshort;

	  /* We have to add bytes which indicate how short the packet
	     is.  We do this by pushing the header backward, which we
	     can do because we allocated two extra bytes for this
	     purpose.  */
	  iclr2 = 0;
	  itt = SHORTDATA;
	  cshort = csize - cdata;
	  if (cshort <= 127)
	    {
	      --zdata;
	      zdata[0] = (char) cshort;
	      zdata[-1] = '\0';
	      if (cshort > 1)
		bzero (zdata + cdata + 1, cshort - 1);
	    }
	  else
	    {
	      zdata -= 2;
	      zdata[0] = (char) (0x80 | (cshort & 0x7f));
	      zdata[1] = (char) (cshort >> 7);
	      bzero (zdata + cdata + 2, cshort - 2);
	      iclr1 = 0;
	    }
	}
    }

  z = zdata - CFRAMELEN;

  /* Zero out the preceding bytes, in case the last time this buffer
     was used those bytes were used.  We need to zero out the initial
     bytes so that we can find the true start of the packet in
     zgadjust_ack.  */
  z[iclr1] = '\0';
  z[iclr2] = '\0';

  z[IFRAME_DLE] = DLE;
  z[IFRAME_K] = (char) iseg;

  icheck = (unsigned short) igchecksum (zdata, csize);

  /* We're just about ready to go.  Wait until there is room in the
     receiver's window for us to send the packet.  We do this now so
     that we send the correct value for the last packet received.
     Note that if iGsendseq == iGremote_ack, this means that the
     sequence numbers are actually 8 apart, since the packet could not
     have been acknowledged before it was sent; this can happen when
     the window size is 7.  */
  while (iGsendseq == iGremote_ack
	 || CSEQDIFF (iGsendseq, iGremote_ack) > iGremote_winsize)
    {
      if (! fgwait_for_packet (qdaemon, TRUE, cGtimeout, cGretries))
	return FALSE;
    }

  /* Ack all packets up to the next one, since the UUCP protocol
     requires that all packets be acked in order.  */
  while (CSEQDIFF (iGrecseq, iGlocal_ack) > 1)
    {
      iGlocal_ack = INEXTSEQ (iGlocal_ack);
      if (! fgsend_control (qdaemon, RR, iGlocal_ack))
	return FALSE;
    }
  iGlocal_ack = iGrecseq;

  z[IFRAME_CONTROL] = (char) ((itt << 6) | (iGsendseq << 3) | iGrecseq);

  iGsendseq = INEXTSEQ (iGsendseq);

  icheck = ((unsigned short)
	    ((0xaaaa - (icheck ^ (z[IFRAME_CONTROL] & 0xff))) & 0xffff));
  z[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  z[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  z[IFRAME_XOR] = (char) (z[IFRAME_K] ^ z[IFRAME_CHECKLOW]
			  ^ z[IFRAME_CHECKHIGH] ^ z[IFRAME_CONTROL]);

  /* If we're waiting for acks of retransmitted packets, then don't
     send this packet yet.  The other side may not be ready for it
     yet.  Instead, code in fggot_ack will send the outstanding
     packets when an ack is received.  */
  ++cGsent_packets;

  if (iGretransmit_seq != -1)
    {
      ++cGdelayed_packets;
      return TRUE;
    }

  DEBUG_MESSAGE2 (DEBUG_PROTO,
		  "fgsenddata: Sending packet %d (%d bytes)",
		  CONTROL_XXX (z[IFRAME_CONTROL]), cdata);

  return fsend_data (qdaemon->qconn, z, CFRAMELEN + csize, TRUE);
}

/* Recompute the control byte and checksum of a packet so that it
   includes the correct packet acknowledgement.  This is called when a
   packet is retransmitted to make sure the retransmission does not
   confuse the other side.  It returns a pointer to the start of the
   packet, skipping the bytes that may be unused at the start of
   azGsendbuffers[iseq].  */

static char *
zgadjust_ack (iseq)
     int iseq;
{
  register char *z;
  unsigned short icheck;

  z = azGsendbuffers[iseq];
  if (*z == '\0')
    ++z;
  if (*z == '\0')
    ++z;

  /* If the received packet number is the same, there is nothing
     to do.  */
  if (CONTROL_YYY (z[IFRAME_CONTROL]) == iGrecseq)
    return z;

  /* Get the old checksum.  */
  icheck = (unsigned short) (((z[IFRAME_CHECKHIGH] & 0xff) << 8)
			     | (z[IFRAME_CHECKLOW] & 0xff));
  icheck = ((unsigned short)
	    (((0xaaaa - icheck) ^ (z[IFRAME_CONTROL] & 0xff)) & 0xffff));

  /* Update the control byte.  */
  z[IFRAME_CONTROL] = (char) ((z[IFRAME_CONTROL] &~ 07) | iGrecseq);

  /* Create the new checksum.  */
  icheck = ((unsigned short)
	    ((0xaaaa - (icheck ^ (z[IFRAME_CONTROL] & 0xff))) & 0xffff));
  z[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  z[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  /* Update the XOR byte.  */
  z[IFRAME_XOR] = (char) (z[IFRAME_K] ^ z[IFRAME_CHECKLOW]
			  ^ z[IFRAME_CHECKHIGH] ^ z[IFRAME_CONTROL]);

  return z;
}

/* Send a control packet.  These are fairly simple to construct.  It
   seems reasonable to me that we should be able to send a control
   packet at any time, even if the receive window is closed.  In
   particular, we don't want to delay when sending a CLOSE control
   message.  If I'm wrong, it can be changed easily enough.  */

static boolean
fgsend_control (qdaemon, ixxx, iyyy)
     struct sdaemon *qdaemon;
     int ixxx;
     int iyyy;
{
  char ab[CFRAMELEN];
  int ictl;
  unsigned short icheck;

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_PROTO) ||
      (FDEBUGGING (DEBUG_ABNORMAL) && ixxx != RR))
    ulog (LOG_DEBUG, "fgsend_control: Sending control %s %d",
	  azGcontrol[ixxx], iyyy);
#endif

  ab[IFRAME_DLE] = DLE;
  ab[IFRAME_K] = KCONTROL;

  ictl = (CONTROL << 6) | (ixxx << 3) | iyyy;
  icheck = (unsigned short) (0xaaaa - ictl);
  ab[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  ab[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  ab[IFRAME_CONTROL] = (char) ictl;

  ab[IFRAME_XOR] = (char) (ab[IFRAME_K] ^ ab[IFRAME_CHECKLOW]
			   ^ ab[IFRAME_CHECKHIGH] ^ ab[IFRAME_CONTROL]);

  return fsend_data (qdaemon->qconn, ab, (size_t) CFRAMELEN, TRUE);
}

/* Wait for data to come in.  This continues processing until a
   complete file or command has been received.  */

boolean
fgwait (qdaemon)
     struct sdaemon *qdaemon;
{
  return fgwait_for_packet (qdaemon, FALSE, cGtimeout, cGretries);
}

/* Get a packet.  This is called when we have nothing to send, but
   want to wait for a packet to come in.  If freturncontrol is TRUE,
   this will return after getting any control packet.  Otherwise, it
   will continue to receive packets until a complete file or a
   complete command has been received.  The timeout and the number of
   retries are specified as arguments.  The function returns FALSE if
   an error occurs or if cretries timeouts of ctimeout seconds were
   exceeded.  */

static boolean
fgwait_for_packet (qdaemon, freturncontrol, ctimeout, cretries)
     struct sdaemon *qdaemon;
     boolean freturncontrol;
     int ctimeout;
     int cretries;
{
  int ctimeouts;
  int cgarbage;
  int cshort;

  ctimeouts = 0;
  cgarbage = 0;
  cshort = 0;

  while (TRUE)
    {
      boolean fexit;
      size_t cneed;
      boolean ffound;
      size_t crec;
  
      if (! fgprocess_data (qdaemon, TRUE, freturncontrol, &fexit,
			    &cneed, &ffound))
	return FALSE;

      if (fexit)
	return TRUE;

      DEBUG_MESSAGE1 (DEBUG_PROTO,
		      "fgwait_for_packet: Need %lu bytes",
		      (unsigned long) cneed);

      if (ffound)
	{
	  ctimeouts = 0;
	  cgarbage = 0;
	}
      else
	{
	  if (cgarbage > cGgarbage_data)
	    {
	      ulog (LOG_ERROR, "Too much unrecognized data");
	      return FALSE;
	    }
	}

      if (! freceive_data (qdaemon->qconn, cneed, &crec, ctimeout, TRUE))
	return FALSE;

      cgarbage += crec;

      if (crec != 0)
	{
	  /* If we don't get enough data twice in a row, we may have
	     dropped some data and still be looking for the end of a
	     large packet.  Incrementing iPrecstart will force
	     fgprocess_data to skip that packet and look through the
	     rest of the data.  In some situations, this will be a
	     mistake.  */
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
	  /* The read timed out.  If we have an unacknowledged packet,
	     send it again.  Otherwise, send an RJ with the last
	     packet we received correctly.  */
	  ++ctimeouts;
	  if (ctimeouts > cretries)
	    {
	      if (cretries > 0)
		ulog (LOG_ERROR, "Timed out waiting for packet");
	      return FALSE;
	    }

	  if (INEXTSEQ (iGremote_ack) != iGsendseq)
	    {
	      int inext;
	      char *zsend;

	      inext = INEXTSEQ (iGremote_ack);

	      DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fgwait_for_packet: Resending packet %d",
			      inext);

	      ++cGresent_packets;
	      zsend = zgadjust_ack (inext);
	      if (! fsend_data (qdaemon->qconn, zsend,
				CFRAMELEN + CPACKLEN (zsend), TRUE))
		return FALSE;
	      iGretransmit_seq = inext;
	    }
	  else
	    {
	      /* Send all pending acks first, to avoid confusing
		 the other side.  */
	      if (iGlocal_ack != iGrecseq)
		{
		  if (! fgsend_acks (qdaemon))
		    return FALSE;
		}
	      if (! fgsend_control (qdaemon, RJ, iGrecseq))
		return FALSE;
	    }
	}
    }
}

/* Send acks for all packets we haven't acked yet.  */

static boolean
fgsend_acks (qdaemon)
     struct sdaemon *qdaemon;
{
  while (iGlocal_ack != iGrecseq)
    {
      iGlocal_ack = INEXTSEQ (iGlocal_ack);
      if (! fgsend_control (qdaemon, RR, iGlocal_ack))
	return FALSE;
    }
  return TRUE;
}

/* Handle an ack of a packet.  According to Hanrahan's paper, this
   acknowledges all previous packets.  If this is an ack for a
   retransmitted packet, continue by resending up to two more packets
   following the retransmitted one.  This should recover quickly from
   a line glitch, while avoiding the problem of continual
   retransmission.  */

static boolean
fggot_ack (qdaemon, iack)
     struct sdaemon *qdaemon;
     int iack;
{
  int inext;
  char *zsend;

  /* We only decrement the error level if we are not retransmitting
     packets.  We want to catch a sudden downgrade in line quality as
     fast as possible.  */
  if (cGerror_level > 0
      && iGretransmit_seq == -1
      && cGsent_packets % cGerror_decay == 0)
    --cGerror_level;
  cGexpect_bad_order = 0;

  /* Each time packet 0 is acknowledged, we call uwindow_acked since a
     new window has been acked.  */
  if (iack < iGremote_ack)
    uwindow_acked (qdaemon, FALSE);

  iGremote_ack = iack;

  if (iGretransmit_seq == -1)
    return TRUE;

  inext = INEXTSEQ (iGretransmit_seq);
  if (inext == iGsendseq)
    iGretransmit_seq = -1;
  else
    {
      DEBUG_MESSAGE1 (DEBUG_PROTO,
		      "fggot_ack: Sending packet %d", inext);

      ++cGresent_packets;
      zsend = zgadjust_ack (inext);
      if (! fsend_data (qdaemon->qconn, zsend, CFRAMELEN + CPACKLEN (zsend),
			TRUE))
	return FALSE;
      inext = INEXTSEQ (inext);
      if (inext == iGsendseq)
	iGretransmit_seq = -1;
      else
	{
	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "fggot_ack: Sending packet %d", inext);

	  ++cGresent_packets;
	  zsend = zgadjust_ack (inext);
	  if (! fsend_data (qdaemon->qconn, zsend,
			    CFRAMELEN + CPACKLEN (zsend), TRUE))
	    return FALSE;
	  iGretransmit_seq = inext;
	}
    }

  return TRUE;
}

/* See if we've received more than the permitted number of errors.  If
   we receive a bad packet, we can expect a window full (less one) of
   out of order packets to follow, so we discount cGbad_order
   accordingly.  */

static boolean
fgcheck_errors (qdaemon)
     struct sdaemon *qdaemon;
{
  if (cGerror_level > cGmax_errors && cGmax_errors >= 0)
    {
      ulog (LOG_ERROR, "Too many '%c' protocol errors",
	    qdaemon->qproto->bname);
      return FALSE;
    }

  return TRUE;
}

/* Process the receive buffer into a data packet, if possible.  All
   control packets are handled here.  When a data packet is received,
   fgprocess_data calls fgot_data with the data; if that sets its
   pfexit argument to TRUE fgprocess_data will set *pfexit to TRUE and
   return TRUE.  Also, if the freturncontrol argument is TRUE
   fgprocess_data will set *pfexit to TRUE and return TRUE.  Otherwise
   fgprocess_data will continue trying to process data.  If some error
   occurs, fgprocess_data will return FALSE.  If there is not enough
   data to form a complete packet, then *pfexit will be set to FALSE,
   *pcneed will be set to the number of bytes needed to form a
   complete packet (unless pcneed is NULL) and fgprocess_data will
   return TRUE.  If this function found a data packet, and pffound is
   not NULL, it will set *pffound to TRUE; this can be used to tell
   valid data from an endless stream of garbage and control packets.
   If fdoacks is TRUE, received packets will be acknowledged;
   otherwise they must be acknowledged later.  */

static boolean
fgprocess_data (qdaemon, fdoacks, freturncontrol, pfexit, pcneed, pffound)
     struct sdaemon *qdaemon;
     boolean fdoacks;
     boolean freturncontrol;
     boolean *pfexit;
     size_t *pcneed;
     boolean *pffound;
{
  *pfexit = FALSE;
  if (pffound != NULL)
    *pffound = FALSE;

  while (iPrecstart != iPrecend)
    {
      char ab[CFRAMELEN];
      int i, iget, cwant;
      unsigned short ihdrcheck, idatcheck;
      const char *zfirst, *zsecond;
      int cfirst, csecond;
      boolean fduprr;

      /* Look for the DLE which must start a packet.  */
      if (abPrecbuf[iPrecstart] != DLE)
	{
	  char *zdle;

	  cfirst = iPrecend - iPrecstart;
	  if (cfirst < 0)
	    cfirst = CRECBUFLEN - iPrecstart;

	  zdle = memchr (abPrecbuf + iPrecstart, DLE, (size_t) cfirst);

	  if (zdle == NULL)
	    {
	      iPrecstart = (iPrecstart + cfirst) % CRECBUFLEN;
	      continue;
	    }

	  /* We don't need % CRECBUFLEN here because zdle - (abPrecbuf
	     + iPrecstart) < cfirst <= CRECBUFLEN - iPrecstart.  */
	  iPrecstart += zdle - (abPrecbuf + iPrecstart);
	}

      /* Get the first six bytes into ab.  */
      for (i = 0, iget = iPrecstart;
	   i < CFRAMELEN && iget != iPrecend;
	   i++, iget = (iget + 1) % CRECBUFLEN)
	ab[i] = abPrecbuf[iget];

      /* If there aren't six bytes, there is no packet.  */
      if (i < CFRAMELEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CFRAMELEN - i;
	  return TRUE;
	}

      /* Make sure these six bytes start a packet.  The check on
	 IFRAME_DLE is basically a debugging check, since the above
	 code should have ensured that it will never fail.  If this is
	 not the start of a packet, bump iPrecstart and loop around to
	 look for another DLE.  */
      if (ab[IFRAME_DLE] != DLE
	  || ab[IFRAME_K] < 1
	  || ab[IFRAME_K] > 9
	  || ab[IFRAME_XOR] != (ab[IFRAME_K] ^ ab[IFRAME_CHECKLOW]
				^ ab[IFRAME_CHECKHIGH] ^ ab[IFRAME_CONTROL])
	  || CONTROL_TT (ab[IFRAME_CONTROL]) == ALTCHAN)
	{
	  ++cGbad_hdr;
	  ++cGerror_level;

	  DEBUG_MESSAGE4 (DEBUG_PROTO | DEBUG_ABNORMAL,
			  "fgprocess_data: Bad header: K %d TT %d XOR byte %d calc %d",
			  ab[IFRAME_K] & 0xff,
			  CONTROL_TT (ab[IFRAME_CONTROL]),
			  ab[IFRAME_XOR] & 0xff,
			  (ab[IFRAME_K]
			   ^ ab[IFRAME_CHECKLOW]
			   ^ ab[IFRAME_CHECKHIGH]
			   ^ ab[IFRAME_CONTROL]) & 0xff);

	  if (! fgcheck_errors (qdaemon))
	    return FALSE;

	  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	  continue;
	}

      /* The zfirst and cfirst pair point to the first set of data for
	 this packet; the zsecond and csecond point to the second set,
	 in case the packet wraps around the end of the buffer.  */
      zfirst = abPrecbuf + iPrecstart + CFRAMELEN;
      cfirst = 0;
      zsecond = NULL;
      csecond = 0;

      if (ab[IFRAME_K] == KCONTROL)
	{
	  /* This is a control packet.  It should not have any data.  */
	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    {
	      ++cGbad_hdr;
	      ++cGerror_level;

	      DEBUG_MESSAGE0 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fgprocess_data: Bad header: control packet with data");

	      if (! fgcheck_errors (qdaemon))
		return FALSE;

	      iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	      continue;
	    }

	  idatcheck = (unsigned short) (0xaaaa - ab[IFRAME_CONTROL]);
	  cwant = 0;
	}
      else
	{
	  int cinbuf;
	  unsigned short icheck;

	  /* This is a data packet.  It should not be type CONTROL.  */
	  if (CONTROL_TT (ab[IFRAME_CONTROL]) == CONTROL)
	    {
	      ++cGbad_hdr;
	      ++cGerror_level;

	      DEBUG_MESSAGE0 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fgprocess_data: Bad header: data packet is type CONTROL");

	      if (! fgcheck_errors (qdaemon))
		return FALSE;

	      iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	      continue;
	    }

	  cinbuf = iPrecend - iPrecstart;
	  if (cinbuf < 0)
	    cinbuf += CRECBUFLEN;
	  cinbuf -= CFRAMELEN;

	  /* Make sure we have enough data.  If we don't, wait for
	     more.  */	     

	  cwant = (int) CPACKLEN (ab);
	  if (cinbuf < cwant)
	    {
	      if (pcneed != NULL)
		*pcneed = cwant - cinbuf;
	      return TRUE;
	    }
	  
	  /* Set up the data pointers and compute the checksum.  */
	  if (iPrecend >= iPrecstart)
	    cfirst = cwant;
	  else
	    {
	      cfirst = CRECBUFLEN - (iPrecstart + CFRAMELEN);
	      if (cfirst >= cwant)
		cfirst = cwant;
	      else if (cfirst > 0)
		{
		  zsecond = abPrecbuf;
		  csecond = cwant - cfirst;
		}
	      else
		{
		  /* Here cfirst is non-positive, so subtracting from
		     abPrecbuf will actually skip the appropriate number
		     of bytes at the start of abPrecbuf.  */
		  zfirst = abPrecbuf - cfirst;
		  cfirst = cwant;
		}
	    }

	  if (csecond == 0)
	    icheck = (unsigned short) igchecksum (zfirst, (size_t) cfirst);
	  else
	    icheck = (unsigned short) igchecksum2 (zfirst, (size_t) cfirst,
						   zsecond,
						   (size_t) csecond);

	  idatcheck = ((unsigned short)
		       (((0xaaaa - (icheck ^ (ab[IFRAME_CONTROL] & 0xff)))
			 & 0xffff)));
	}
      
      ihdrcheck = (unsigned short) (((ab[IFRAME_CHECKHIGH] & 0xff) << 8)
				    | (ab[IFRAME_CHECKLOW] & 0xff));

      if (ihdrcheck != idatcheck)
	{
	  DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			  "fgprocess_data: Bad checksum: header 0x%x, data 0x%x",
			  ihdrcheck, idatcheck);

	  ++cGbad_checksum;
	  ++cGerror_level;

	  if (! fgcheck_errors (qdaemon))
	    return FALSE;

	  /* If the checksum failed for a data packet, then if it was
	     the one we were expecting send an RJ, otherwise ignore
	     it.  Previously if this code got the wrong packet number
	     it would send an RR, but that may confuse some Telebit
	     modems and it doesn't help in any case since the receiver
	     will probably just ignore the RR as a duplicate (that's
	     basically what this code does).  If we totally missed the
	     packet we will time out and send an RJ in the function
	     fgwait_for_packet above.  */
	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    {
	      /* Make sure we've acked everything up to this point.  */
	      if (iGrecseq != iGlocal_ack)
		{
		  if (! fgsend_acks (qdaemon))
		    return FALSE;
		}

	      /* If this is the packet we wanted, tell the sender that
		 it failed.  */
	      if (CONTROL_XXX (ab[IFRAME_CONTROL]) == INEXTSEQ (iGrecseq))
		{
		  if (! fgsend_control (qdaemon, RJ, iGrecseq))
		    return FALSE;
		  cGexpect_bad_order += iGrequest_winsize - 1;
		}
	    }

	  /* We can't skip the packet data after this, because if we
	     have lost incoming bytes the next DLE will be somewhere
	     in what we thought was the packet data.  */
	  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
	  continue;
	}

      /* We have a packet; remove the processed bytes from the receive
	 buffer.  */
      iPrecstart = (iPrecstart + cwant + CFRAMELEN) % CRECBUFLEN;

      /* Store the control byte for the handshake routines.  */
      iGpacket_control = ab[IFRAME_CONTROL] & 0xff;

      /* Annoyingly, some UUCP packages appear to send an RR packet
	 rather than an RJ packet when they want a packet to be
	 resent.  If we get a duplicate RR and we've never seen an RJ,
	 we treat the RR as an RJ.  */
      fduprr = FALSE;
      if (cGremote_rejects == 0
	  && CONTROL_TT (ab[IFRAME_CONTROL]) == CONTROL
	  && CONTROL_XXX (ab[IFRAME_CONTROL]) == RR
	  && iGremote_ack == CONTROL_YYY (ab[IFRAME_CONTROL])
	  && INEXTSEQ (iGremote_ack) != iGsendseq
	  && iGretransmit_seq != -1)
	{
	  DEBUG_MESSAGE0 (DEBUG_PROTO | DEBUG_ABNORMAL,
			  "fgprocess_data: Treating duplicate RR as RJ");
	  fduprr = TRUE;
	}

      /* Update the received sequence number from the yyy field of a
	 data packet (if it is the one we are expecting) or an RR
	 control packet.  If we've been delaying sending packets until
	 we received an ack, this may send out some packets.  */
      if ((CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL
	   && CONTROL_XXX (ab[IFRAME_CONTROL]) == INEXTSEQ (iGrecseq))
	  || (CONTROL_XXX (ab[IFRAME_CONTROL]) == RR && ! fduprr))
	{
	  if (! fggot_ack (qdaemon, CONTROL_YYY (ab[IFRAME_CONTROL])))
	    return FALSE;
	}

      /* If this isn't a control message, make sure we have received
	 the expected packet sequence number, acknowledge the packet
	 if it's the right one, and process the data.  */
      if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	{
	  if (CONTROL_XXX (ab[IFRAME_CONTROL]) != INEXTSEQ (iGrecseq))
	    {
	      /* We got the wrong packet number.  */
	      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fgprocess_data: Got packet %d; expected %d",
			      CONTROL_XXX (ab[IFRAME_CONTROL]),
			      INEXTSEQ (iGrecseq));

	      if (cGexpect_bad_order > 0)
		--cGexpect_bad_order;
	      else
		{
		  ++cGbad_order;
		  ++cGerror_level;
		  if (! fgcheck_errors (qdaemon))
		    return FALSE;
		}

	      /* This code used to send an RR to encourage the other
		 side to get back in synch, but that may confuse some
		 Telebit modems and does little good in any case,
		 since the other side will probably just ignore it
		 anyhow (that's what this code does).  */
	      continue;
	    }

	  /* We got the packet we expected.  */
	  ++cGrec_packets;
	  if (cGerror_level > 0
	      && cGrec_packets % cGerror_decay == 0)
	    --cGerror_level;
	  cGexpect_bad_order = 0;

	  iGrecseq = INEXTSEQ (iGrecseq);

	  DEBUG_MESSAGE1 (DEBUG_PROTO,
			  "fgprocess_data: Got packet %d", iGrecseq);

	  /* Tell the caller that we found something.  */
	  if (pffound != NULL)
	    *pffound = TRUE;

	  /* If we are supposed to do acknowledgements here, send back
	     an RR packet.  */
	  if (fdoacks)
	    {
	      if (! fgsend_acks (qdaemon))
		return FALSE;
	    }

	  /* If this is a short data packet, adjust the data pointers
	     and lengths.  */
	  if (CONTROL_TT (ab[IFRAME_CONTROL]) == SHORTDATA)
	    {
	      int cshort, cmove;

	      if ((zfirst[0] & 0x80) == 0)
		{
		  cshort = zfirst[0] & 0xff;
		  cmove = 1;
		}
	      else
		{
		  int cbyte2;

		  if (cfirst > 1)
		    cbyte2 = zfirst[1] & 0xff;
		  else
		    cbyte2 = zsecond[0] & 0xff;
		  cshort = (zfirst[0] & 0x7f) + (cbyte2 << 7);
		  cmove = 2;
		}

	      DEBUG_MESSAGE1 (DEBUG_PROTO,
			      "fgprocess_data: Packet short by %d",
			      cshort);

	      /* Adjust the start of the buffer for the bytes used
		 by the count.  */
	      if (cfirst > cmove)
		{
		  zfirst += cmove;
		  cfirst -= cmove;
		}
	      else
		{
		  zfirst = zsecond + (cmove - cfirst);
		  cfirst = csecond - (cmove - cfirst);
		  csecond = 0;
		}

	      /* Adjust the length of the buffer for the bytes we are
		 not supposed to consider.  */
	      cshort -= cmove;
	      if (csecond >= cshort)
		csecond -= cshort;
	      else
		{
		  cfirst -= cshort - csecond;
		  csecond = 0;
		}

#if DEBUG > 0
	      /* This should not happen, but just in case.  */
	      if (cfirst < 0)
		cfirst = 0;
#endif
	    }

	  if (! fgot_data (qdaemon, zfirst, (size_t) cfirst,
			   zsecond, (size_t) csecond,
			   -1, -1, (long) -1,
			   INEXTSEQ (iGremote_ack) == iGsendseq,
			   pfexit))
	    return FALSE;

	  /* If fgot_data told us that we were finished, get out.  */
	  if (*pfexit)
	    return TRUE;

	  /* If we've been asked to return control packets, get out
	     now.  */
	  if (freturncontrol)
	    {
	      *pfexit = TRUE;
	      return TRUE;
	    }

	  continue;
	}

      /* Handle control messages here. */
#if DEBUG > 1
      if (FDEBUGGING (DEBUG_PROTO)
	  || (FDEBUGGING (DEBUG_ABNORMAL)
	      && CONTROL_XXX (ab[IFRAME_CONTROL]) != RR))
	ulog (LOG_DEBUG, "fgprocess_data: Got control %s %d",
	      azGcontrol[CONTROL_XXX (ab[IFRAME_CONTROL])],
	      CONTROL_YYY (ab[IFRAME_CONTROL]));
#endif

      switch (CONTROL_XXX (ab[IFRAME_CONTROL]))
	{
	case CLOSE:
	  /* The other side has closed the connection.  */
	  if (fLog_sighup)
	    {
	      ulog (LOG_ERROR, "Received unexpected CLOSE packet");
	      (void) fgsend_control (qdaemon, CLOSE, 0);
	    }
	  return FALSE;
	case RR:
	  /* Acknowledge receipt of a packet.  This was already handled
	     above, unless we are treating it as RJ.  */
	  if (! fduprr)
	    break;
	  /* Fall through.  */
	case RJ:
	  /* The other side dropped a packet.  Begin retransmission with
	     the packet following the one acknowledged.  We don't
	     retransmit the packets immediately, but instead wait
	     for the first one to be acked.  This prevents us from
	     sending an entire window several times if we get several
	     RJ packets.  */
	  iGremote_ack = CONTROL_YYY (ab[IFRAME_CONTROL]);
	  iGretransmit_seq = INEXTSEQ (iGremote_ack);
	  if (iGretransmit_seq == iGsendseq)
	    iGretransmit_seq = -1;
	  else
	    {
	      char *zpack;

	      DEBUG_MESSAGE2 (DEBUG_PROTO | DEBUG_ABNORMAL,
			      "fgprocess_data: Remote reject: next %d resending %d",
			      iGsendseq, iGretransmit_seq);

	      ++cGresent_packets;
	      if (fduprr)
		++cGremote_duprrs;
	      else
		++cGremote_rejects;
	      ++cGerror_level;
	      if (! fgcheck_errors (qdaemon))
		return FALSE;
	      zpack = zgadjust_ack (iGretransmit_seq);
	      if (! fsend_data (qdaemon->qconn, zpack,
				CFRAMELEN + CPACKLEN (zpack),
				TRUE))
		return FALSE;
	    }
	  break;
	case SRJ:
	  /* Selectively reject a particular packet.  This is not used
	     by UUCP, but it's easy to support.  */
	  DEBUG_MESSAGE1 (DEBUG_PROTO | DEBUG_ABNORMAL,
			  "fgprocess_data: Selective reject of %d",
			  CONTROL_YYY (ab[IFRAME_CONTROL]));
	  {
	    char *zpack;

	    ++cGresent_packets;
	    ++cGremote_rejects;
	    ++cGerror_level;
	    zpack = zgadjust_ack (CONTROL_YYY (ab[IFRAME_CONTROL]));
	    if (! fsend_data (qdaemon->qconn, zpack,
			      CFRAMELEN + CPACKLEN (zpack),
			      TRUE))
	      return FALSE;
	  }
	  break;
	case INITC:
	case INITB:
	case INITA:
	  /* Ignore attempts to reinitialize.  */
	  break;
	}

      /* If we've been asked to return control packets, get out.  */
      if (freturncontrol)
	{
	  *pfexit = TRUE;
	  return TRUE;
	}

      /* Loop around to look for the next packet, if any.  */
    }

  /* There is no data left in the receive buffer.  */
  if (pcneed != NULL)
    *pcneed = CFRAMELEN;
  return TRUE;
}

/* Compute the 'g' protocol checksum.  This is unfortunately rather
   awkward.  This is the most time consuming code in the entire
   program.  It's also not a great checksum, since it can be fooled
   by some single bit errors.  */

/* Sorry about this knavery, but it speeds up the VAX code
   significantly.  It would be better to rewrite the whole routine in
   assembler.  */
#ifdef __GNUC__
#ifdef __vax__
#define VAX_ASM 1
#endif
#endif

#if VAX_ASM
#define ROTATE(i) \
  asm ("cvtwl %1,%0\n\trotl $1,%0,%0" : "=g" (i) : "g" (i))
#else
#define ROTATE(i) i += i + ((i & 0x8000) >> 15)
#endif

#define ITERATION \
      /* Rotate ichk1 left.  */ \
      ROTATE (ichk1); \
 \
      /* The guts of the checksum.  */ \
      b = BUCHAR (*z++); \
      if (b != 0) \
	{ \
	  ichk1 &= 0xffff; \
	  ichk1 += b; \
	  ichk2 += ichk1 ^ c; \
	  if ((ichk1 >> 16) != 0) \
	    ichk1 ^= ichk2; \
	} \
      else \
	{ \
	  ichk2 += ichk1 ^ c; \
	  ichk1 ^= ichk2; \
	} \
 \
      --c

static int
igchecksum (z, c)
     register const char *z;
     register size_t c;
{
  register unsigned long ichk1, ichk2;

  ichk1 = 0xffff;
  ichk2 = 0;

  do
    {
      register unsigned int b;

      ITERATION;
      ITERATION;
      ITERATION;
      ITERATION;
    }
  while (c > 0);

  return ichk1 & 0xffff;
}

/* We use a separate function compute the checksum if the block is
   split around the end of the receive buffer since it occurs much
   less frequently and the checksum is already high up in the
   profiles.  These functions are almost identical, and this one
   actually only has a few more instructions in the inner loop.  */

static int
igchecksum2 (zfirst, cfirst, zsecond, csecond)
     const char *zfirst;
     size_t cfirst;
     const char *zsecond;
     size_t csecond;
{
  register unsigned long ichk1, ichk2;
  register const char *z;
  register size_t c;

  z = zfirst;
  c = cfirst + csecond;

  ichk1 = 0xffff;
  ichk2 = 0;

  do
    {
      register unsigned int b;

      ITERATION;

      /* If the first buffer has been finished, switch to the second.  */
      --cfirst;
      if (cfirst == 0)
	z = zsecond;
    }
  while (c > 0);

  return ichk1 & 0xffff;
}
