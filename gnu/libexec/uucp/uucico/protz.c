/* protz.c		Version 1.5, 92Apr24 */
/* Modified by Ian Lance Taylor for Taylor UUCP 1.04 92Aug4.  */

/*
 * Doug Evans, dje@sspiff.UUCP or dje@ersys.edmonton.ab.ca
 *
 * This file provides the Zmodem protocol (by Chuck Forsberg) for
 * Ian Taylor's UUCP package.
 *
 * It was originally developed to establish a uucp link between myself and my
 * employer: Ivation Datasystems, Inc. of Ottawa. 
 *
 * My thanks to Ivation for letting me release this to the public. Given that
 * Zmodem is in the public domain, no additional copyrights have been added.
 *
 *****************************************************************************
 *
 * It's been difficult fitting Zmodem into the UUCP world. I have been guided
 * mostly by trying to plug it into Taylor UUCP. Where "the Zmodem way of doing
 * things" conflicted with "the UUCP way of doing things", I have err'd on the
 * side of UUCP. At the end of it all, I have achieved something that will plug
 * into Taylor UUCP very easily, but some might argue that I have corrupted Z
 * too much. At any rate, compatibility with sz/rz was sacrificed to achieve a
 * clean UUCP protocol. Given that, I took the opportunity to start from
 * scratch when defining protocol constants (EG: ZBIN).
 *
 * 1) I wasn't quite sure how to enhance Zmodem to handle send+receive in one
 *    session, so I added a 'g' protocol like initialization sequence. This
 *    also gets this stuff out of the way, in case we ever try to support
 *    full-duplex.
 *
 *	Caller			    Callee
 *	------			    ------
 *	ZINIT		-->	<-- ZINIT
 *	ZDATA (ZCRCF)	-->	<-- ZDATA (ZCRCF)
 *	ZACK		-->	<-- ZACK
 *	ZINITEND	-->	<-- ZINITEND
 *
 *    ZINIT is a combination of ZRINIT and ZSINIT and is intended to exchange
 *    simple protocol information (flags) and the protocol version number.
 *    ZDATA is intended to include window size information as well as the
 *    "Myattn" string (although at the moment it doesn't contain anything).
 *    ZDATA may contain at most 1k bytes of data and is sent out as one ZCRCF
 *    packet. Two ack's (ZACK + ZINITEND) are needed to ensure both sides have
 *    received ZDATA.
 *
 * 2) I've hardcoded several protocol parameters, like 32 bit CRC's for data.
 *    Others are not supported (we don't need them).
 *
 * 3) ZHEX headers use 32 bit CRC's.
 *
 * 4) Zmodem sends the ZFILE message "in one chunk". If there are errors, the
 *    entire string is resent. I have continued this practice. All UUCP
 *    commands are sent "in one chunk". This can be changed down the road if
 *    necessary.
 *
 * 5) The ZEOF message has been replaced with a new ZCRCx value: ZCRCF. ZCRCF
 *    is identical to ZCRCW except that it indicates the end of the message.
 *    The protocol here is *not* a file transfer protocol. It is an end to end
 *    transport protocol (that preserves message boundaries).
 *
 * 6) Zmodem handles restarting a file transfer, but as best as I can tell UUCP
 *    does not. At least Taylor UUCP doesn't. And if UUCP does start handling
 *    file restart, can it be plugged into the existing Zmodem way with zero
 *    changes? Beats me. Therefore I have removed this part of the code. One
 *    can always put it back in if and when UUCP handles it. Ditto for other
 *    pieces of removed code: there's no point in overly complicating this code
 *    when supporting all the bells and whistles requires enhancements to UUCP
 *    itself.
 *
 *    *** It is easier to put code back in in an upward compatible manner ***
 *    *** than it is to correct for misunderstood code or poorly merged   ***
 *    *** (Zmodem vs UUCP) code.                                          ***
 *
 * 7) For the character in the initial "protocol selection" sequence, I have
 *    chosen 'a'. I'm told 'z' is already in use for something that isn't
 *    Zmodem. It's entirely reasonable to believe that if Zmodem ever becomes a
 *    standard UUCP protocol, this won't be it (so I'll leave z/Z for them).
 *    Publicly, this is the 'a' protocol. Internally, it is refered to as 'z'.
 *    A little confusing, I know. Maybe in time I'll refer to it internally as
 *    'a', or maybe in time this will be *the* 'z' protocol.
 *
 * 8) Since we are writing a transport protocol, which isn't supposed to know
 *    anything about what is being transfered or where it is coming from, the
 *    header data value has changed meaning. It no longer means "file position"
 *    but instead means "window position". It is a running counter of the bytes
 *    transfered. Each "message" begins on a 1k boundary so the count isn't a
 *    precise byte count. The counter wraps every 4 gigabytes, although this
 *    wrapping isn't supported yet.
 *
 *    FIXME: At present the max data transfered per session is 4 gigabytes.
 *
 ****************************************************************************
 *
 * A typical message sequence is (master sending file to slave):
 *
 *      Master                          Slave
 *      ------                          -----
 *	ZDATA (S, ZCRCF)	-->
 *				<--	ZACK
 *				<--	ZDATA (SY, ZCRCF)
 *	ZACK			-->
 *	ZDATA			-->
 *                        ...	<--	ZACK/ZRPOS
 *	ZDATA (ZCRCF)		-->
 *				<--	ZACK
 *				<--	ZDATA (CY, ZCRCF)
 *	ZACK			-->
 *
 * A typical message sequence is (master receiving file from slave):
 *
 *	Master				Slave
 *	------				-----
 *	ZDATA (R, ZCRCF)	-->
 *				<--	ZACK
 *				<--	ZDATA (RY, ZCRCF)
 *	ZACK			-->
 *				<--	ZDATA
 *	ZACK/ZRPOS	...	-->
 *				<--	ZDATA (ZCRCF)
 *	ZACK			-->
 *	ZDATA (CY, ZCRCF)	-->
 *				<--	ZACK
 *
 *****************************************************************************
 *
 * Notes:
 * 1) For future bidirectional concerns, keep packet types "unidirectional".
 *	Sender always uses:	ZDATA, ZNAK
 *	Receiver always uses:	ZRPOS, ZACK
 *	There is no intersection.
 *
 *    I'm not sure if this is necessary or even useful, but it seems to be.
 *
 * 2) I use to store the byte count / 32 in the data header. This left 5 bits
 *    unused for future concerns. I removed this because of the following
 *    situation when sending a file:
 *
 *	ZDATA (ZCRCG, xx bytes) - received ok
 *	ZDATA (ZCRCF, 0 bytes)  - corrupted
 *
 *    At this point the receiver would like to send back a ZRPOS with a value 
 *    of the size of the file. However, it can't because the value is divided
 *    by 32, and it would have to round up to the next multiple of 32. This
 *    seemed a little ugly, so I went with using the entire header to store
 *    the byte count.
 *
 *****************************************************************************
 *
 * Source version:
 * 
 * 1.1,2,3
 *	Protocol version 0
 *	Early attempts, completely rewritten later.
 *
 * 1.4	Protocol version 1
 *	Beta test sent to Ian for analysis 92Apr18.
 *
 * 1.5	Protocol version 1
 *	Released 92Apr24.
 *
 *****************************************************************************
 *
 * Protocol version:
 *
 * A version number is exchanged in the ZINIT message, so it is possible to
 * correct or enhance the protocol, without breaking existing versions.
 * The purpose of this section is to document these versions as they come out.
 * Remember, this is the protocol version, not the source version.
 *
 * 0	Initial version.
 *	Zmodem controlled file transfer. This was more of a "plug Z
 *	into UUCP as is" port.
 *
 * 1	Complete rewrite.
 *	Made Z more of a transport protocol. UUCP now controls transfer and Z
 *	is on the same footing as the other UUCP protocols.
 *	Theoretically, there will be little pain when UUCP goes bidirectional.
 */

#include "uucp.h"

#if USE_RCS_ID
const char protz_rcsid[] = "$Id: protz.c,v 1.2 1994/05/07 18:13:52 ache Exp $";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

#define ZPROTOCOL_VERSION	1

/*
 * Control message characters ...
 */

#define ZPAD	'*'	/* Padding character begins frames */
#define ZDLE	030	/* Ctrl-X Zmodem escape - `ala BISYNC DLE */
#define ZBIN	'A'	/* Binary frame indicator */
#define ZHEX	'B'	/* HEX frame indicator */

/*
 * Frame types (see array "frametypes" in zm.c) ...
 *
 * Note that the numbers here have been reorganized, as we don't support
 * all of them (nor do we need to).
 *
 * WARNING: The init sequence assumes ZINIT < ZDATA < ZACK < ZINITEND.
 */

#define ZINIT		0	/* Init (contains protocol version, flags) */
#define ZDATA		1	/* Data packet(s) follow */
#define ZRPOS		2	/* Resume data trans at this position */
#define ZACK		3	/* ACK to above */
#define ZNAK		4	/* Last packet was garbled */
#define Zreserved	5	/* reserved (for future concerns) */
#define ZINITEND	6	/* end of init sequence */
#define ZFIN		7	/* Finish session */

/*
 * ZDLE sequences ...
 *
 * Note addition of ZCRCF: "end of message".
 */

#define ZCRCE 'h'	/* CRC next, frame ends, header packet follows */
#define ZCRCG 'i'	/* CRC next, frame continues nonstop */
#define ZCRCQ 'j'	/* CRC next, frame continues, ZACK expected */
#define ZCRCW 'k'	/* CRC next, ZACK expected, end of frame */
#define ZCRCF 'l'	/* CRC next, ZACK expected, end of message */

#define ZRUB0 'm'	/* Translate to rubout 0177 */
#define ZRUB1 'n'	/* Translate to rubout 0377 */


/*
 * zdlread return values (internal) ...
 * Other values are ZM_ERROR, ZM_TIMEOUT, ZM_RCDO.
 */

#define GOTOR	0400
#define GOTCRCE (ZCRCE | GOTOR)	/* ZDLE-ZCRCE received */
#define GOTCRCG (ZCRCG | GOTOR)	/* ZDLE-ZCRCG received */
#define GOTCRCQ (ZCRCQ | GOTOR)	/* ZDLE-ZCRCQ received */
#define GOTCRCW (ZCRCW | GOTOR)	/* ZDLE-ZCRCW received */
#define GOTCRCF (ZCRCF | GOTOR)	/* ZDLE-ZCRCF received */

/*
 * Byte positions within header array ...
 */

#define ZF0	3	/* First flags byte */
#define ZF1	2
#define ZF2	1
#define ZF3	0

#define ZP0	0	/* Low order 8 bits of position */
#define ZP1	1
#define ZP2	2
#define ZP3	3	/* High order 8 bits of position */

/*
 * Bit Masks for ZRQINIT flags byte ZF0 ...
 */

#define TX_ESCCTL	1	/* Tx will escape control chars */

/*
 * Possible errors when running ZMODEM ...
 */

#define	ZM_ERROR	(-1)	/* crc error, etc. */
#define ZM_TIMEOUT	(-2)
#define ZM_RCDO		(-3)	/* Carrier Lost */

/*
 * ASCII characters ...
 */

#define LF		012
#define CR		015
#define XON		021
#define XOFF		023

#define XON_WAIT	10	/* seconds */

/*
 * Packet sizes ...
 *
 * FIXME: CPACKETSIZE is hardcoded in a lot of places.
 *	It's not clear to me whether changing it's value would be a
 *	"good thing" or not. But of course that doesn't excuse the hardcoding.
 */

#define CPACKETSIZE		1024	/* max packet size (data only) */
#define CFRAMELEN		12	/* header size */
#define CSUFFIXLEN		10	/* suffix at end of data packets */
#define CEXCHANGE_INIT_RETRIES	4

/* The header CRC value.  */

#if ANSI_C
#define IHDRCRC 0xDEBB20E3UL
#else
#define IHDRCRC ((unsigned long) 0xDEBB20E3L)
#endif

/* packet buffer size */
#define CPACKBUFSIZE  (CFRAMELEN + 2 * CPACKETSIZE + CSUFFIXLEN + 42 /*slop*/)

/*
 * Data types ...
 */

typedef unsigned char achdrval_t[4];
typedef unsigned long hdrval_t;
typedef unsigned long winpos_t;

/*
 * Configurable parms ...
 *
 * FIXME: <cZrx_buf_len> isn't used yet. It may not be needed.
 */

#define CTIMEOUT		10
#define CRETRIES		10
#define CSTARTUP_RETRIES	4
#define CGARBAGE		2400
#define CSEND_WINDOW		16384
#define FESCAPE_CONTROL		FALSE

static int cZtimeout = CTIMEOUT;	/* (seconds) */
static int cZretries = CRETRIES;
static int cZstartup_retries = CSTARTUP_RETRIES;
static int cZmax_garbage = CGARBAGE;		/* max garbage before header */
static int cZtx_window = CSEND_WINDOW;		/* our transmission window */
static int cZrx_buf_len = 0;			/* our reception buffer size */
static boolean fZesc_ctl = FESCAPE_CONTROL;	/* escape control chars */

struct uuconf_cmdtab asZproto_params[] =
{
	{"timeout", UUCONF_CMDTABTYPE_INT, (pointer) & cZtimeout, NULL},
	{"retries", UUCONF_CMDTABTYPE_INT, (pointer) & cZretries, NULL},
	{"startup-retries", UUCONF_CMDTABTYPE_INT,
	   (pointer) & cZstartup_retries, NULL},
	{"garbage", UUCONF_CMDTABTYPE_INT, (pointer) & cZmax_garbage, NULL},
	{"send-window", UUCONF_CMDTABTYPE_INT, (pointer) & cZtx_window, NULL},
	{"escape-control", UUCONF_CMDTABTYPE_BOOLEAN, (pointer) & fZesc_ctl,
	   NULL},
	{NULL, 0, NULL, NULL}
};

/*
 * Variables for statistic gathering ...
 *
 * We use <wpZtxpos, wpZrxbytes> to record the number of "packets"
 * sent/received. Packets is in double quotes because some of them aren't full.
 */

static unsigned long cZheaders_sent;
static unsigned long cZheaders_received;
static unsigned long cZbytes_resent;
static unsigned long cZtimeouts;
static unsigned long cZerrors;

/*
 * Data buffers ...
 */

static char *zZtx_buf;		/* transmit buffer */

static char *zZtx_packet_buf;	/* raw outgoing packet data */
static char *zZrx_packet_buf;	/* raw incoming packet data */

/*
 * Transmitter state variables ...
 */

static unsigned cZblklen;	/* data length in sent/received packets */
static unsigned cZtxwspac;	/* spacing between ZCRCQ requests */
/*static unsigned cZblklen_override;*//* override value for <cZblklen> */
static unsigned cZtxwcnt;	/* counter used to space ack requests */
static unsigned cZrxwcnt;	/* counter used to watch receiver's buf size */
static winpos_t wpZtxstart;	/* <wpZtxpos> when message started */
static winpos_t wpZtxpos;	/* transmitter position */
static winpos_t wpZlastsync;	/* last offset to which we got a ZRPOS */
static winpos_t wpZlrxpos;	/* receiver's last reported offset */
static winpos_t wpZrxpos;	/* receiver file position */

static int iZlast_tx_data_packet; /* type of last ZDATA packet (ZCRCx) */
static int iZjunk_count;	/* amount of garbage characters received */
static int iZtleft;		/* for dynamic packet resizing */

static int iZbeenhereb4;	/* times we've been ZRPOS'd to same place */

/*
 * Receiver state variables ...
 */

static winpos_t wpZrxbytes;	/* receiver byte count */
static int iZlast_rx_data_packet; /* last successfully received ZCRCx packet */

/*
 * Misc. globals ...
 */

static char xon = XON;

#ifdef DJE_TESTING
int uucptest = -1;
int uucptest2;
int uucptestseed;
#endif

/*
 * Kludge!!!
 * See fzfinish_tx(). Basically the next two globals are used to record the
 * fact that we got a ZDATA, but aren't quite ready to process it.
 */

static int iZpkt_rcvd_kludge;			/* -1 if not valid */
static hdrval_t hvZpkt_hdrval_kludge;

/*
 * Packet types ...
 */

static const char *azZframe_types[] = {
	"Carrier Lost",		/* -3 */
	"Timeout",		/* -2 */
	"Error",		/* -1 */
#define FTOFFSET 3
	"ZINIT",
	"ZDATA",
	"ZRPOS",
	"ZACK",
	"ZNAK",
	"Zreserved",
	"ZINITEND",
	"ZFIN",
	"UNKNOWN!!!"
};
#define FTNUMBER	(sizeof(azZframe_types) / sizeof(char *))

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif
#define ZZHEADER_NAME(itype) \
		azZframe_types[min((itype) + FTOFFSET, FTNUMBER - 1)]

/*
 * Local functions ...
 */

static boolean fzsend_data P((struct sdaemon *qdaemon, char *zdata,
			      size_t cdata, boolean fendofmessage));
static boolean fzprocess P((struct sdaemon *qdaemon));
static boolean fzstart_proto P((struct sdaemon *qdaemon));
static int izexchange_init P((struct sdaemon *qdaemon, int send_type,
			      achdrval_t send_val, achdrval_t recv_val));
static boolean fzshutdown_proto P((struct sdaemon *qdaemon));
static boolean fzstart_tx P((void));
static boolean fzfinish_tx P((struct sdaemon *qdaemon, long *plredo));
static boolean fzstart_rx P((void));
static boolean fzfinish_rx P((struct sdaemon *qdaemon));
static boolean fzsend_hdr P((struct sdaemon *qdaemon, int ipkttype,
			     int ihdrtype, hdrval_t hdrval,
			     boolean fcheckreceive));
static boolean fzsend_data_packet P((struct sdaemon *qdaemon, char *zdata,
				     size_t cdata, int frameend,
				     boolean fcheckreceive));
static int czbuild_header P((char *zresult, int ipkttype, int ihdrtype,
			     hdrval_t hdrval));
static int czbuild_data_packet P((char *zresult, const char *zdata,
				  size_t cdata, int frameend));
/*
 * The rest of the functions do not follow Ian's naming style. I have left
 * the names the same as the original zm source. Over time, they may change.
 */
static int izrecv_hdr P((struct sdaemon *qdaemon, achdrval_t hdr));
static int zrbhdr32 P((struct sdaemon *qdaemon, achdrval_t hdr));
static int zrhhdr P((struct sdaemon *qdaemon, achdrval_t hdr));
static int zrdat32 P((struct sdaemon *qdaemon, char *buf, int length,
		      int *iprxcount));
static int getinsync P((struct sdaemon *qdaemon, boolean flag));
static char *zputhex P((char *p, int ch));
static char *zputchar P((char *p, int ch));
static int zgethex P((struct sdaemon *qdaemon));
static int zdlread P((struct sdaemon *qdaemon));
static int noxrd7 P((struct sdaemon *qdaemon));
static int realreadchar P((struct sdaemon *qdaemon, int timeout));
static boolean fzreceive_ready P((void));
static void stohdr P((hdrval_t pos, achdrval_t hdr));
static hdrval_t rclhdr P((achdrval_t hdr));
static hdrval_t hvzencode_data_hdr P((winpos_t cbytes));
static void zdecode_data_hdr P((hdrval_t hdrval, winpos_t *pcbytes));
static winpos_t lzupdate_rxpos P((achdrval_t rx_hdr, winpos_t rxpos,
				  winpos_t lrxpos, winpos_t txpos));

/*
 * This macro replaces readchar() because it achieves a noticable speed up. The
 * readchar() function has been renamed realreadchar(). Thanks to Ian for
 * running this stuff through a profiler to find this out. Ian suggests further
 * speed ups may be obtained by doing a similar thing in zrdat32().
 */

/* Assign the next character to b. */
#define READCHAR(qdaemon, b, i) \
  (iPrecstart != iPrecend \
   ? ((b) = BUCHAR (abPrecbuf[iPrecstart]), \
      iPrecstart = (iPrecstart + 1) % CRECBUFLEN) \
   : ((b) = realreadchar ((qdaemon), (i))))

/************************************************************************/


/*
 * Start the protocol ...
 */

boolean
fzstart(qdaemon, pzlog)
struct sdaemon *qdaemon;
char **pzlog;
{
	*pzlog = zbufalc (sizeof "protocol 'a' starting: , , , , , " + 100);
	sprintf (*pzlog, "protocol 'a' starting: %d, %d, %d, %d, %d, %d",
		cZtimeout, cZretries, cZstartup_retries,
		cZmax_garbage, cZtx_window, fZesc_ctl);

        if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
			 STRIPSETTING_EIGHTBITS, XONXOFF_OFF))
	        return FALSE;

	/*
	 * For now, we place tight restrictions on the size of the transmit
	 * window. This might be relaxed in the future. If it is relaxed,
	 * some of these tests will stay, some will go. That is why it is
	 * coded like it is.
	 */

	if (cZtx_window % 1024 != 0 ||
		cZtx_window < 4096 || cZtx_window > 65536 ||
		65536 % cZtx_window != 0
	) {
		ulog (LOG_ERROR,
	   "fzstart: cZtx_window not one of 4096, 8192, 16384, 32768, 65536");
		return FALSE;
	}

	zZtx_buf = (char *) xmalloc (CPACKETSIZE);
	zZtx_packet_buf = (char *) xmalloc (CPACKBUFSIZE);
	zZrx_packet_buf = (char *) xmalloc (CPACKBUFSIZE);

	iZlast_tx_data_packet = -1;
	iZlast_rx_data_packet = -1;

	wpZtxpos = wpZlrxpos = wpZrxpos = wpZrxbytes = 0;
	cZtxwspac = cZtx_window / 4;

	cZheaders_sent = cZheaders_received = cZbytes_resent = 0;
	cZtimeouts = cZerrors = 0;

	iZpkt_rcvd_kludge = -1;

#if 0
	/*
	 * We ensure <cZtx_window> is at least 4k, so the following is
	 * unnecessary. It can be put back in later if needed.
	 */
	if (cZblklen_override > cZtxwspac
	    || (!cZblklen_override && cZtxwspac < 1024))
		cZblklen_override = cZtxwspac;
#endif

#ifdef DJE_TESTING
	{
		extern int uucptest,uucptest2,uucptestseed;
		FILE *f;

		if (uucptest == -1) {
			f = fopen ("/usr/local/src/bin/uucp/uucptest", "r");
			if (f != NULL) {
				fscanf (f, "%d %d %d",
					&uucptestseed, &uucptest, &uucptest2);
				fclose (f);
			}
			srand (uucptestseed);
		}
	}
#endif

	/*
	 * Fire up the protocol (exchange init messages) ...
	 */

	if (!fzstart_proto (qdaemon))
		return FALSE;

	return TRUE;
}

/*
 * Stop the protocol ...
 */

boolean
fzshutdown(qdaemon)
struct sdaemon *qdaemon;
{
	(void) fzshutdown_proto (qdaemon);

	xfree ((pointer) zZtx_buf);
	xfree ((pointer) zZtx_packet_buf);
	xfree ((pointer) zZrx_packet_buf);
	zZtx_buf = NULL;
	zZtx_packet_buf = NULL;
	zZrx_packet_buf = NULL;

	/*
	 * Print some informative statistics ...
	 *
	 * I use the word "messages" here instead of "headers" because the
	 * latter is jargonese.
	 */

	ulog (LOG_NORMAL,
	      "Protocol 'a' messages: sent %lu, received %lu",
	      cZheaders_sent, cZheaders_received);
	ulog (LOG_NORMAL,
	      "Protocol 'a' packets: sent %lu, received %lu",
	      wpZtxpos / 1024, wpZrxbytes / 1024);
	if (cZbytes_resent != 0 || cZtimeouts != 0 || cZerrors != 0)
		ulog (LOG_NORMAL,
	    "Protocol 'a' errors: bytes resent %lu, timeouts %lu, errors %lu",
		      cZbytes_resent, cZtimeouts, cZerrors);

	/*
	 * Reset all the parameters to their default values, so that the
	 * protocol parameters used for this connection do not affect the
	 * next one.
	 */

	cZtimeout = CTIMEOUT;
	cZretries = CRETRIES;
	cZstartup_retries = CSTARTUP_RETRIES;
	cZmax_garbage = CGARBAGE;
	cZtx_window = CSEND_WINDOW;
	fZesc_ctl = FESCAPE_CONTROL;

	cZheaders_sent = cZheaders_received = cZbytes_resent = 0;
	cZtimeouts = cZerrors = 0;

	return TRUE;
}

/*
 * Send a command string ...
 * We send everything up to and including the null byte.
 *
 * We assume the command will fit in the outgoing data buffer.
 * FIXME: A valid assumption?
 */

/*ARGSUSED*/
boolean
fzsendcmd(qdaemon, z, ilocal, iremote)
struct sdaemon *qdaemon;
const char *z;
int ilocal;
int iremote;
{
	size_t n,clen;
	long lredo;
	char *zbuf;

	clen = strlen (z) + 1;

	DEBUG_MESSAGE1 (DEBUG_PROTO, "fzsendcmd: sending command %s", z);

	if (!fzstart_tx ())	/* must be called before zzgetspace() */
		return FALSE;

	if ((zbuf = zzgetspace (qdaemon, &n)) == NULL)
		return FALSE;

#if DEBUG > 0
	if (clen > n)
		ulog (LOG_FATAL, "fzsendcmd: clen > n");
#endif

	strcpy (zbuf, z);

	/*
	 * Send it out ...
	 */

	do {
		if (!fzsend_data (qdaemon, zbuf, clen, TRUE))
			return FALSE;
		if (!fzfinish_tx (qdaemon, &lredo))
			return FALSE;
	} while (lredo >= 0);

	return fzprocess (qdaemon);
}

/*
 * Allocate a packet to send out ...
 *
 * Note that 'z' has dynamic packet resizing and that <cZblklen> will range
 * from 32 to 1024, in multiples of 2.
 */

/*ARGSUSED*/
char *
zzgetspace(qdaemon, pclen)
struct sdaemon *qdaemon;
size_t *pclen;
{
	*pclen = cZblklen;
	return zZtx_buf;
}

/*
 * Send a block of data ...
 *
 * If (cdata == 0) then the end of the file has been reached.
 */

/*ARGSUSED*/
boolean
fzsenddata(qdaemon, zdata, cdata, ilocal, iremote, ipos)
struct sdaemon *qdaemon;
char *zdata;
size_t cdata;
int ilocal;
int iremote;
long ipos;
{
	DEBUG_MESSAGE1 (DEBUG_PROTO, "fzsenddata: %d bytes", cdata);

	if (! fzsend_data (qdaemon, zdata, cdata, cdata == 0))
		return FALSE;
	return fzprocess (qdaemon);
}

/*
 * Send a block of data (command or file) ...
 */

/* This should buffer the data internally.  Until it does, it needs to
   be able to reset the file position when it is called.  This is
   really ugly.  */
extern struct stransfer *qTsend;

static boolean
fzsend_data(qdaemon, zdata, cdata, fendofmessage)
struct sdaemon *qdaemon;
char *zdata;
size_t cdata;
boolean fendofmessage;
{
	size_t n;

	if (iZlast_tx_data_packet == -1 || iZlast_tx_data_packet == ZCRCW) {
		cZtxwcnt = cZrxwcnt = 0;
		iZjunk_count = 0;
		if (!fzsend_hdr (qdaemon, ZBIN, ZDATA,
				 hvzencode_data_hdr (wpZtxpos), TRUE))
			return FALSE;
	}

	n = cdata;

	if (fendofmessage)
		iZlast_tx_data_packet = ZCRCF;
	else if (iZjunk_count > 3)
		iZlast_tx_data_packet = ZCRCW;
	else if (wpZtxpos == wpZlastsync)
		iZlast_tx_data_packet = ZCRCW;
	else if (cZrx_buf_len && (cZrxwcnt += n) >= cZrx_buf_len)
		iZlast_tx_data_packet = ZCRCW;
	else if ((cZtxwcnt += n) >= cZtxwspac) {
		iZlast_tx_data_packet = ZCRCQ;
		cZtxwcnt = 0;
	} else
		iZlast_tx_data_packet = ZCRCG;

	if (++iZtleft > 3) {
		iZtleft = 0;
		if (cZblklen < 1024)
			cZblklen *= 2;
#if 0	/* <cZblklen_override> is currently unnecessary */
		if (cZblklen_override && cZblklen > cZblklen_override)
			cZblklen = cZblklen_override;
#endif
		if (cZblklen > 1024)
			cZblklen = 1024;
		if (cZrx_buf_len && cZblklen > cZrx_buf_len)
			cZblklen = cZrx_buf_len;
	}

#if DEBUG > 1
	if (FDEBUGGING(DEBUG_PROTO)) {
		const char *type;

		switch (iZlast_tx_data_packet) {
		case ZCRCW: type = "ZCRCW"; break;
		case ZCRCG: type = "ZCRCG"; break;
		case ZCRCQ: type = "ZCRCQ"; break;
		case ZCRCE: type = "ZCRCE"; break;
		case ZCRCF: type = "ZCRCF"; break;
		default : type = "UNKNOWN!!!"; break;
		}
		DEBUG_MESSAGE3 (DEBUG_PROTO,
				"fzsend_data: %s, pos 0x%lx, %d bytes",
				type, wpZtxpos, n);
	}
#endif

	if (!fzsend_data_packet (qdaemon, zdata, n, iZlast_tx_data_packet,
				 TRUE))
		return FALSE;

	wpZtxpos += n;

	if (iZlast_tx_data_packet == ZCRCW) {
		/*
		 * FIXME: Ideally this would be done in fzprocess. However, it
		 *	is only called if there is data pending which there
		 *	may not be yet. I could have patched fploop() a bit but
		 *	for now, I've done it like this.
		 */
		switch (getinsync (qdaemon, FALSE)) {
		case ZACK:
			break;
		case ZRPOS:
			if (qTsend == NULL
			    || ! ffileisopen (qTsend->e)) {
				ulog (LOG_ERROR, "Can't reset non-file");
				return FALSE;
			}
			iZlast_tx_data_packet = -1; /* trigger ZDATA */
			DEBUG_MESSAGE1 (DEBUG_PROTO,
					"fzsend_data: Seeking to %ld",
					(long) (wpZrxpos - wpZtxstart));
			if (!ffileseek (qTsend->e, wpZrxpos - wpZtxstart)) {
				ulog (LOG_ERROR, "seek: %s", strerror (errno));
				return FALSE;
			}
			break;
		default:
			return FALSE;
		}
		return TRUE;
	}

	/*
	 * If we've reached the maximum transmit window size, let the
	 * receiver catch up ...
	 *
	 * I use (cZtx_window - 2048) to play it safe.
	 */

	while (wpZtxpos - wpZlrxpos >= cZtx_window - 2048) {
		if (iZlast_tx_data_packet != ZCRCQ) {
		    if (!fzsend_data_packet (qdaemon, zdata, (size_t) 0,
					     iZlast_tx_data_packet = ZCRCQ,
					     TRUE))
				return FALSE;
		}
		/*
		 * FIXME: I'd rather not call ffileseek() in this file. When we
		 *	start buffering the outgoing data, the following
		 *	ffileseek() will disappear.
		 */
		switch (getinsync (qdaemon, TRUE)) {
		case ZACK:
			break;
		case ZRPOS:
			if (qTsend == NULL
			    || ! ffileisopen (qTsend->e)) {
				ulog (LOG_ERROR, "Can't reset non-file");
				return FALSE;
			}
			iZlast_tx_data_packet = -1; /* trigger ZDATA */
			DEBUG_MESSAGE1 (DEBUG_PROTO,
					"fzsend_data: Seeking to %ld",
					(long) (wpZrxpos - wpZtxstart));
			if (!ffileseek (qTsend->e, wpZrxpos - wpZtxstart)) {
				ulog (LOG_ERROR, "seek: %s", strerror (errno));
				return FALSE;
			}
			break;
		default:
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * Process existing data ...
 */

static boolean
fzprocess(qdaemon)
struct sdaemon *qdaemon;
{
	int c,ch;

	while (fzreceive_ready ()) {
		READCHAR (qdaemon, ch, 1);
		switch (ch) {
		case ZPAD:
			/* see if we're detecting ZRPOS packets quickly */
			DEBUG_MESSAGE0 (DEBUG_PROTO,
					"fzprocess: possible ZRPOS packet");
			/* We just ate the ZPAD char that getinsync
			   expects, so put it back.  */
			iPrecstart = ((iPrecstart + CRECBUFLEN - 1)
				      % CRECBUFLEN);
			c = getinsync (qdaemon, TRUE);
			if (c == ZACK)
				break;
			/* FIXME: sz does a TCFLSH here */
#if 0	/* FIXME: Not sure if this is needed, or where to put it. */
			/* ZCRCE - dinna wanna starta ping-pong game */
			if (!fzsend_data_packet (qdaemon, zZtx_packet_buf,
						 0, ZCRCE, TRUE))
				return FALSE;
#endif
			if (c == ZRPOS) {
				if (qTsend == NULL
				    || ! ffileisopen (qTsend->e)) {
					ulog (LOG_ERROR,
					      "Attempt to back up non-file");
					return FALSE;
				}
				if (! ffileseek (qTsend->e,
						 wpZrxpos - wpZtxstart)) {
					ulog (LOG_ERROR,
					      "seek: %s", strerror (errno));
					return FALSE;
				}
				iZlast_tx_data_packet = -1; /* trigger ZDATA */
				break;	/* not returning is intentional */
			}
			return FALSE;
		case XOFF:
		case XOFF | 0200:
			READCHAR (qdaemon, ch, XON_WAIT);
			break;
		case CR:
			break;
		default:
			iZjunk_count++;
			break;
		}
	}

	return TRUE;
}

/*
 * Wait for data to come in.
 *
 * This continues processing until a complete file or command has been
 * received.
 */

boolean
fzwait(qdaemon)
struct sdaemon *qdaemon;
{
	int c,cerr,rxcount;
	boolean fexit;
	achdrval_t rx_hdr;

	if (!fzstart_rx ())
		return FALSE;

	cerr = cZretries;

	goto nxthdr;

	for (;;) {
		if (!fzsend_hdr (qdaemon, ZHEX, ZRPOS,
				 hvzencode_data_hdr (wpZrxbytes), FALSE))
			return FALSE;
nxthdr:
		c = izrecv_hdr (qdaemon, rx_hdr);

		switch (c) {
		case ZM_TIMEOUT:
		case ZNAK:
			if (--cerr < 0) {
				ulog (LOG_ERROR, "fzwait: retries exhausted");
				return FALSE;
			}
			continue;
		case ZM_ERROR:
			if (--cerr < 0) {
				ulog (LOG_ERROR, "fzwait: retries exhausted");
				return FALSE;
			}
			/*fport_break ();*/
			continue;
		case ZM_RCDO:
		case ZFIN:
			return FALSE;
		case ZRPOS:
		case ZACK:
			goto nxthdr;	/* ignore, partner is out of sync */
		case ZDATA: {
			winpos_t rx_bytes;

			zdecode_data_hdr (rclhdr (rx_hdr), &rx_bytes);
			DEBUG_MESSAGE2 (DEBUG_PROTO,
				"fzwait: bytes(us,them) 0x%lx,0x%lx",
				wpZrxbytes, rx_bytes);
			if (rx_bytes != wpZrxbytes) {
				if (--cerr < 0) {
					ulog (LOG_ERROR,
					      "fzwait: retries exhausted");
					return FALSE;
				}
				(void) zrdat32 (qdaemon, zZrx_packet_buf,
						1024, &rxcount);
				/*fport_break ();*/
				/*
				 * FIXME: Seems to me we should ignore this one
				 *	and go for a timeout, the theory being
				 *	that the appropriate ZRPOS has already
				 *	been sent. We're obviously out of sync.
				 *	/dje 92Mar10
				 */
				continue;	/* goto nxthdr? */
			}
moredata:
			/*
			 * Do not call fgot_data() with (rxcount == 0) if it's
			 * not ZCRCF. fgot_data() will erroneously think this
			 * is the end of the message.
			 */
			c = zrdat32 (qdaemon, zZrx_packet_buf, 1024,
				     &rxcount);
#if DEBUG > 1
			if (FDEBUGGING(DEBUG_PROTO)) {
				const char *msg;

				if (c < 0) {
					msg = ZZHEADER_NAME(c);
				} else {
					switch (c) {
					case GOTCRCW: msg = "ZCRCW"; break;
					case GOTCRCG: msg = "ZCRCG"; break;
					case GOTCRCQ: msg = "ZCRCQ"; break;
					case GOTCRCE: msg = "ZCRCE"; break;
					case GOTCRCF: msg = "ZCRCF"; break;
					default : msg = NULL; break;
					}
				}
				if (msg != NULL)
					DEBUG_MESSAGE2 (DEBUG_PROTO,
					      "fzwait: zrdat32: %s, %d bytes",
							msg, rxcount);
				else
					DEBUG_MESSAGE2 (DEBUG_PROTO,
					      "fzwait: zrdat32: %d, %d bytes",
							c, rxcount);
			}
#endif
			switch (c) {
			case ZM_ERROR:	/* CRC error */
				cZerrors++;
				if (--cerr < 0) {
					ulog (LOG_ERROR,
					      "fzwait: retries exhausted");
					return FALSE;
				}
				/*fport_break ();*/
				continue;
			case ZM_TIMEOUT:
				cZtimeouts++;
				if (--cerr < 0) {
					ulog (LOG_ERROR,
					      "fzwait: retries exhausted");
					return FALSE;
				}
				continue;
			case ZM_RCDO:
				return FALSE;
			case GOTCRCW:
				iZlast_rx_data_packet = ZCRCW;
				cerr = cZretries;
				if (rxcount != 0
				    && !fgot_data (qdaemon, zZrx_packet_buf,
						   (size_t) rxcount,
						   (const char *) NULL,
						   (size_t) 0,
						   -1, -1, (long) -1,
						   TRUE, &fexit))
					return FALSE;
				wpZrxbytes += rxcount;
				if (!fzsend_hdr (qdaemon, ZHEX, ZACK,
					     hvzencode_data_hdr (wpZrxbytes),
					     FALSE))
					return FALSE;
				if (! fsend_data (qdaemon->qconn, &xon,
						  (size_t) 1, FALSE))
				  return FALSE;
				goto nxthdr;
			case GOTCRCQ:
				iZlast_rx_data_packet = ZCRCQ;
				cerr = cZretries;
				if (rxcount != 0
				    && !fgot_data (qdaemon, zZrx_packet_buf,
						   (size_t) rxcount,
						   (const char *) NULL,
						   (size_t) 0,
						   -1, -1, (long) -1,
						   TRUE, &fexit))
					return FALSE;
				wpZrxbytes += rxcount;
				if (!fzsend_hdr (qdaemon, ZHEX, ZACK,
					     hvzencode_data_hdr (wpZrxbytes),
					     FALSE))
					return FALSE;
				goto moredata;
			case GOTCRCG:
				iZlast_rx_data_packet = ZCRCG;
				cerr = cZretries;
				if (rxcount != 0
				    && !fgot_data (qdaemon, zZrx_packet_buf,
						   (size_t) rxcount,
						   (const char *) NULL,
						   (size_t) 0,
						   -1, -1, (long) -1,
						   TRUE, &fexit))
					return FALSE;
				wpZrxbytes += rxcount;
				goto moredata;
			case GOTCRCE:
				iZlast_rx_data_packet = ZCRCE;
				cerr = cZretries;
				if (rxcount != 0
				    && !fgot_data (qdaemon, zZrx_packet_buf,
						   (size_t) rxcount,
						   (const char *) NULL,
						   (size_t) 0,
						   -1, -1, (long) -1,
						   TRUE, &fexit))
					return FALSE;
				wpZrxbytes += rxcount;
				goto nxthdr;
			case GOTCRCF:
				iZlast_rx_data_packet = ZCRCF;
				/*
				 * fzfinish_rx() must be called before
				 * fgot_data() because fgot_data() will send
				 * out a UUCP-command but the sender won't be
				 * ready for it until it receives our final
				 * ZACK.
				 */
				cerr = cZretries;
				wpZrxbytes += rxcount;
				if (!fzfinish_rx (qdaemon))
					return FALSE;
				if (!fgot_data (qdaemon, zZrx_packet_buf,
						(size_t) rxcount,
						(const char *) NULL,
						(size_t) 0, -1, -1,
						(long) -1, TRUE, &fexit))
					return FALSE;
				/*
				 * FIXME: Examine <fexit>?
				 * Or maybe ensure it's TRUE?
				 */
				return TRUE;
			}
			return FALSE;
		}
		default:
			ulog (LOG_FATAL, "fzwait: received header %s",
				ZZHEADER_NAME(c));
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * File level routine. Called when initiating/terminating file transfers.
 *
 * When starting to send a file:	(TRUE, TRUE, cbytes)
 * When starting to receive a file:	(TRUE, FALSE, -1)
 * When send EOF, check resend:		(FALSE, TRUE, -1)
 * When receive EOF, check re-receive:	(FALSE, FALSE, -1)
 */

boolean
fzfile(qdaemon, qtrans, fstart, fsend, cbytes, pfhandled)
struct sdaemon *qdaemon;
struct stransfer *qtrans;
boolean fstart;
boolean fsend;
long cbytes;
boolean *pfhandled;
{
	long iredo;

	*pfhandled = FALSE;

	DEBUG_MESSAGE2 (DEBUG_PROTO, "fzfile: fstart=%d, fsend=%d", fstart,
			fsend);

	if (fsend) {
		if (fstart)
			return fzstart_tx ();
		if (! fzfinish_tx (qdaemon, &iredo))
			return FALSE;
		if (iredo >= 0) {
			if (! ffileisopen (qtrans->e)) {
				ulog (LOG_ERROR,
				      "Attempt to back up non-file");
				return FALSE;
			}
			if (! ffileseek (qtrans->e, iredo)) {
				ulog (LOG_ERROR,
				      "seek: %s", strerror (errno));
				return FALSE;
			}
			*pfhandled = TRUE;
			qtrans->fsendfile = TRUE;
			return fqueue_send (qdaemon, qtrans);
		}
	}

	return TRUE;
}

/****************************************************************************/


#if 0	/* not used, we only use 32 bit crc's */
/*
 * crctab calculated by Mark G. Mendel, Network Systems Corporation
 */

static unsigned short crctab[256] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};
#endif	/* crctab */

/*
 * Copyright (C) 1986 Gary S. Brown.  You may use this program, or
 * code or tables extracted from it, as desired without restriction.
 */

/* First, the polynomial itself and its table of feedback terms.  The  */
/* polynomial is                                                       */
/* X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0 */
/* Note that we take it "backwards" and put the highest-order term in  */
/* the lowest-order bit.  The X^32 term is "implied"; the LSB is the   */
/* X^31 term, etc.  The X^0 term (usually shown as "+1") results in    */
/* the MSB being 1.                                                    */

/* Note that the usual hardware shift register implementation, which   */
/* is what we're using (we're merely optimizing it by doing eight-bit  */
/* chunks at a time) shifts bits into the lowest-order term.  In our   */
/* implementation, that means shifting towards the right.  Why do we   */
/* do it this way?  Because the calculated CRC must be transmitted in  */
/* order from highest-order term to lowest-order term.  UARTs transmit */
/* characters in order from LSB to MSB.  By storing the CRC this way,  */
/* we hand it to the UART in the order low-byte to high-byte; the UART */
/* sends each low-bit to hight-bit; and the result is transmission bit */
/* by bit from highest- to lowest-order term without requiring any bit */
/* shuffling on our part.  Reception works similarly.                  */

/* The feedback terms table consists of 256, 32-bit entries.  Notes:   */
/*                                                                     */
/*     The table can be generated at runtime if desired; code to do so */
/*     is shown later.  It might not be obvious, but the feedback      */
/*     terms simply represent the results of eight shift/xor opera-    */
/*     tions for all combinations of data and CRC register values.     */
/*                                                                     */
/*     The values must be right-shifted by eight bits by the "updcrc"  */
/*     logic; the shift must be unsigned (bring in zeroes).  On some   */
/*     hardware you could probably optimize the shift in assembler by  */
/*     using byte-swap instructions.                                   */

static unsigned long crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
    0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
    0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
    0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
    0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
    0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
    0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
    0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
    0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
    0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
    0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
    0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
    0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
    0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
    0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
    0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
    0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
    0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
    0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
    0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
    0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
    0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
    0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
    0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
    0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
    0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
    0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
    0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
    0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
    0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
    0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
    0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
    0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
    0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
    0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
    0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
    0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
    0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
    0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
    0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
    0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
    0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
    0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
    0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
    0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
    0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
    0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
    0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
    0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
    0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
    0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
    0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

/*
 * updcrc macro derived from article Copyright (C) 1986 Stephen Satchell. 
 *  NOTE: First argument must be in range 0 to 255.
 *        Second argument is referenced twice.
 * 
 * Programmers may incorporate any or all code into their programs, 
 * giving proper credit within the source. Publication of the 
 * source routines is permitted so long as proper credit is given 
 * to Stephen Satchell, Satchell Evaluations and Chuck Forsberg, 
 * Omen Technology.
 */

#define updcrc(cp, crc) (crctab[((crc >> 8) & 255)] ^ (crc << 8) ^ cp)

#define UPDC32(b, crc) \
  (crc_32_tab[((unsigned)(crc) ^ (unsigned)(b)) & 0xff] \
   ^ (((crc) >> 8) & 0x00ffffffL))

/****************************************************************************/

/*
 * This section contains the guts of the Zmodem protocol. The intention
 * is to leave as much of it alone as possible at the start. Overtime it
 * will be cleaned up (EG: I'd like to clean up the naming of the globals).
 * Also, Zmodem has a different coding style. Over time this will be converted
 * to the Taylor UUCP coding style.
 */

/*
 * Start the protocol (exchange init packets) ...
 *
 * UUCP can transfer files in both directions in one session. Therefore the
 * init sequence is a little different.
 *
 * 1) ZINIT packets are exchanged
 *    - contains protocol version and protocol flags
 * 2) ZDATA packets are exchanged
 *    - is intended to contain various numeric and string information
 * 3) ZACK packets are exchanged
 * 4) ZINITEND packets are exchanged
 */

static boolean
fzstart_proto(qdaemon)
struct sdaemon *qdaemon;
{
	int i;
	achdrval_t tx_hdr,rx_hdr;

	for (i = 0; i < cZstartup_retries; i++) {
		stohdr (0L, tx_hdr);
		tx_hdr[ZF0] = ZPROTOCOL_VERSION;
		if (fZesc_ctl)
			tx_hdr[ZF1] |= TX_ESCCTL;
		switch (izexchange_init (qdaemon, ZINIT, tx_hdr, rx_hdr)) {
		case -1: return FALSE;
		case 0:  continue;
		case 1:  break;
		}
#if 0	/* can't work, but kept for documentation */
		if (rx_hdr[ZF0] == 0) {
			ulog (LOG_ERROR, "Old protocol version, init failed");
			return FALSE;
		}
#endif
		fZesc_ctl = fZesc_ctl || (rx_hdr[ZF1] & TX_ESCCTL) != 0;

		stohdr (0L, tx_hdr);
		switch (izexchange_init (qdaemon, ZDATA, tx_hdr, rx_hdr)) {
		case -1: return FALSE;
		case 0:  continue;
		case 1:  break;
		}

		stohdr (0L, tx_hdr);
		switch (izexchange_init (qdaemon, ZACK, tx_hdr, rx_hdr)) {
		case -1: return FALSE;
		case 0:  continue;
		case 1:  break;
		}

		stohdr (0L, tx_hdr);
		switch (izexchange_init (qdaemon, ZINITEND, tx_hdr, rx_hdr)) {
		case -1: return FALSE;
		case 0:  continue;
		case 1:  break;
		}

		DEBUG_MESSAGE0 (DEBUG_PROTO,
				"fzstart_proto: Protocol started");
		return TRUE;

		/* FIXME: see protg.c regarding sequencing here. */
	}

	ulog (LOG_ERROR, "Protocol init failed");
	return FALSE;
}

/*
 * Exchange init messages. This is based on 'g'.
 * See the comments concerning fgexchange_init() in protg.c.
 *
 * We return 1 for success, 0 for restart, -1 for comm failure (terminate).
 */

static int
izexchange_init(qdaemon, send_type, send_val, recv_val)
struct sdaemon *qdaemon;
int send_type;
achdrval_t send_val;
achdrval_t recv_val;
{
	int i,recv_type,count;

	for (i = 0; i < CEXCHANGE_INIT_RETRIES; i++) {
		if (!fzsend_hdr (qdaemon, send_type == ZDATA ? ZBIN : ZHEX,
				 send_type, rclhdr (send_val), FALSE))
			return -1;

		/*
		 * The ZDATA packet is intended to contain the <Attn> string
		 * (eventually, if it's ever usable) and allow for anything
		 * else that will need to be thrown in.
		 */

		if (send_type == ZDATA) {
			count = czbuild_data_packet (zZtx_packet_buf, "",
						     (size_t) 1, ZCRCF);
			if (!fsend_data (qdaemon->qconn, zZtx_packet_buf,
					 (size_t) count, FALSE))
				return -1;
		}

		recv_type = izrecv_hdr (qdaemon, recv_val);

		switch (recv_type) {
		case ZM_TIMEOUT:
		case ZM_ERROR:
			continue;
		case ZM_RCDO:
		case ZFIN:
			return -1;
		case ZINIT:
		case ZACK:
		case ZINITEND:
			break;
		case ZDATA:
			if (zrdat32 (qdaemon, zZrx_packet_buf, 1024, &count)
			    == GOTCRCF)
				break;
			continue;
		default:
			continue;
		}

		if (recv_type == send_type)
			return 1;

		/*
		 * If the other side is farther along than we are, we have lost
		 * a packet.  Fall immediately back to ZINIT (but don't fail
		 * if we are already doing ZINIT, since that would count
		 * against cStart_retries more than it should).
		 *
		 * FIXME: The ">" test is "<" in protg.c. Check who's right.
		 */

		if (recv_type > send_type && send_type != ZINIT)
			return 0;

		/*
		 * If we are sending ZINITEND and we receive an ZINIT, the
		 * other side has falled back (we know this because we have
		 * seen a ZINIT from them).  Fall back ourselves to start
		 * the whole handshake over again.
		 */

		if (recv_type == ZINIT && send_type == ZINITEND)
			return 0;
	}

	return 0;
}

/*
 * Shut down the protocol ...
 */

static boolean
fzshutdown_proto(qdaemon)
struct sdaemon *qdaemon;
{
	(void) fzsend_hdr (qdaemon, ZHEX, ZFIN, 0L, FALSE);
	return TRUE;
}

/*
 * Reset the transmitter side for sending a new message ...
 */

static boolean
fzstart_tx()
{
	iZlast_tx_data_packet = -1;

	/*
	 * <wpZlastsync> is set to -1L to suppress ZCRCW request otherwise
	 * triggered by (wpZlastsync == wpZtxpos).
	 */

	cZblklen = 1024;
	wpZlastsync = -1L;
	iZbeenhereb4 = 0;
	iZtleft = 0;
	iZjunk_count = 0;

	wpZtxpos = (wpZtxpos + 1024L) & ~1023L;	/* next packet boundary */
	wpZlrxpos = wpZrxpos = wpZtxpos;

	wpZtxstart = wpZtxpos;	/* so we can compute the "file offset" */

	return TRUE;
}

/*
 * Finish the sending of a message ...
 *
 * Basically, we wait for some indication that the receiver received our last
 * message. If the receiver tells us to restart from some point, we set
 * *plredo to that point.
 *
 * FIXME: This function is a major kludge at the moment. It is taken from
 *	getinsync(). It is necessary because I don't yet buffer outgoing data.
 *	It will go away when we do (buffer outgoing data).
 */

static boolean
fzfinish_tx(qdaemon, plredo)
struct sdaemon *qdaemon;
long *plredo;
{
	int c,cerr,ctimeouts;
	achdrval_t rx_hdr;
	winpos_t rx_bytes;

	*plredo = -1;
	cerr = cZretries;
	ctimeouts = 0;

	DEBUG_MESSAGE4 (DEBUG_PROTO,
	  "fzfinish_tx: txpos=0x%lx, rxpos=0x%lx, lrxpos=0x%lx, rxbytes=0x%lx",
			wpZtxpos, wpZrxpos, wpZlrxpos, wpZrxbytes);

	for (;;) {
		c = izrecv_hdr (qdaemon, rx_hdr);

		switch (c) {
		case ZRPOS:
			wpZrxpos = lzupdate_rxpos (rx_hdr, wpZrxpos,
						   wpZlrxpos, wpZtxpos);
			/*
			 * If the receiver sends a ZRPOS for the 1k block after
			 * the one we're currently at, we lost the final ZACK.
			 * We cheat and ignore this ZRPOS. Remember: the theory
			 * is that this entire function will go away when we
			 * begin buffering the outgoing data. Of course, one
			 * can reword the protocol definition and say this
			 * isn't cheating at all.
			 */
			if (((wpZtxpos + 1024) & ~1023) == wpZrxpos)
				return TRUE;
			cZbytes_resent += wpZtxpos - wpZrxpos;
			wpZlrxpos = wpZtxpos = wpZrxpos;
			if (wpZlastsync == wpZrxpos) {
				if (++iZbeenhereb4 > 4)
					if (cZblklen > 32)
						cZblklen /= 2;
				/* FIXME: shouldn't we reset iZbeenhereb4? */
			}
			wpZlastsync = wpZrxpos;
			iZlast_tx_data_packet = ZCRCW; /* force a timeout */
			*plredo = wpZrxpos - wpZtxstart;
			return TRUE;
		case ZACK:
			wpZrxpos = lzupdate_rxpos (rx_hdr, wpZrxpos,
						   wpZlrxpos, wpZtxpos);
			wpZlrxpos = wpZrxpos;
			if (wpZtxpos == wpZrxpos)  /* the ACK we want? */
				return TRUE;
			break;
		case ZDATA:
			/*
			 * We cheat here and take advantage of UUCP's current
			 * half duplex nature. If we get a ZDATA starting on
			 * the next 1k boundary, we lost the ZACK. We cheat and
			 * tuck it away so that izrecv_hdr() can later detect
			 * it. Remember: see above.
			 */
			zdecode_data_hdr (rclhdr (rx_hdr), &rx_bytes);
			if (((wpZrxbytes + 1024L) & ~1023L) == rx_bytes) {
				iZpkt_rcvd_kludge = ZDATA;
				hvZpkt_hdrval_kludge = rclhdr (rx_hdr);
				return TRUE;
			}
			break;	/* ignore, out of sync (old) */
		case ZNAK:
			/*
			 * We cheat here and take advantage of UUCP's current
			 * half duplex nature. If we get a ZNAK starting on
			 * the next 1k boundary, we lost the ZACK. We cheat and
			 * throw the ZNAK away. Remember: see above.
			 *
			 * On the other hand, if (rx_bytes == wpZrxbytes) then
			 * the other side is also in fzfinish_tx(). He must
			 * have lost our ZACK, so we send him another.
			 */
			zdecode_data_hdr (rclhdr (rx_hdr), &rx_bytes);
			if (((wpZrxbytes + 1024L) & ~1023L) == rx_bytes)
				return TRUE;
			if (rx_bytes == wpZrxbytes) {
				if (!fzsend_hdr (qdaemon, ZHEX, ZACK,
					     hvzencode_data_hdr (wpZrxbytes),
					     TRUE))
					return FALSE;
			}
			break;	/* ignore, out of sync (old) */
		case ZFIN:
		case ZM_RCDO:
			return FALSE;
		case ZM_TIMEOUT:
			if (--cerr < 0) {
				ulog (LOG_ERROR,
				      "fzfinish_tx: retries exhausted");
				return FALSE;
			}
			/*
			 * Normally the sender doesn't send NAK's for timeouts.
			 * We have to here because of the following scenario:
			 *
			 * - We send ZDATA/ZCRCF
			 * - They send ZACK (corrupted)
			 * - They send ZDATA/ZCRCF (corrupted)
			 *
			 * At this point, both sides are in fzfinish_tx().
			 * We only send ZNAK every second timeout to increase
			 * our timeout delay vs. our partner. This tries to
			 * avoid ZRPOS and ZNAK "passing in transit".
			 */
			if (++ctimeouts % 2 == 0)
				if (!fzsend_hdr (qdaemon, ZHEX, ZNAK,
						 hvzencode_data_hdr (wpZtxpos),
						 TRUE))
					return FALSE;
			break;
		case ZM_ERROR:
		default:
			if (--cerr < 0) {
				ulog (LOG_ERROR,
				      "fzfinish_tx: retries exhausted");
				return FALSE;
			}
			if (!fzsend_hdr (qdaemon, ZHEX, ZNAK,
					 hvzencode_data_hdr (wpZtxpos),
					 TRUE))
				return FALSE;
			break;
		}
	}
}

/*
 * Initialize the receiver ...
 */

static boolean
fzstart_rx()
{
	wpZrxbytes = (wpZrxbytes + 1024L) & ~1023L; /* next packet boundary */

	return TRUE;
}

/*
 * Terminate the receiver ...
 *
 * Acknowledge the last packet received.
 */

static boolean
fzfinish_rx(qdaemon)
struct sdaemon *qdaemon;
{
	DEBUG_MESSAGE0 (DEBUG_PROTO, "fzfinish_rx: message/file received");

	return fzsend_hdr (qdaemon, ZHEX, ZACK,
			   hvzencode_data_hdr (wpZrxbytes), FALSE);
}

/*
 * Send a Zmodem header to our partner ...
 */

static boolean
fzsend_hdr(qdaemon, ipkttype, ihdrtype, hdrval, fcheckreceive)
struct sdaemon *qdaemon;
int ipkttype;
int ihdrtype;
hdrval_t hdrval;
boolean fcheckreceive;
{
	int cpacketlen;

	DEBUG_MESSAGE2 (DEBUG_PROTO, "fzsend_hdr: %s, data = 0x%lx",
			ZZHEADER_NAME(ihdrtype), hdrval);

	cpacketlen = czbuild_header (zZtx_packet_buf, ipkttype,
				     ihdrtype, hdrval);

#ifdef DJE_TESTING
#if 0
	if (ihdrtype == ZACK && rand () % 100 < uucptest2) {
		cZheaders_sent++;
		return TRUE;
	}
#else
	if (ihdrtype == ZACK || ihdrtype == ZDATA) {
		boolean fresult;
		int old;
		extern int uucptest,uucptest2;

		old = uucptest;
		uucptest = uucptest2;
		cZheaders_sent++;
		fresult = fsend_data (qdaemon->qconn, zZtx_packet_buf,
				      (size_t) cpacketlen, fcheckreceive);
		uucptest = old;
		return fresult;
	}
#endif
#endif
	cZheaders_sent++;
	return fsend_data (qdaemon->qconn, zZtx_packet_buf,
			   (size_t) cpacketlen, fcheckreceive);
}

/*
 * Send a data packet to our partner ...
 * <frameend> is one of ZCRCx.
 */

static boolean
fzsend_data_packet(qdaemon, zdata, cdata, frameend, fcheckreceive)
struct sdaemon *qdaemon;
char *zdata;
size_t cdata;
int frameend;
boolean fcheckreceive;
{
	int cpacketlen;

	cpacketlen = czbuild_data_packet (zZtx_packet_buf, zdata, cdata,
					  frameend);

	return fsend_data (qdaemon->qconn, zZtx_packet_buf,
			   (size_t) cpacketlen, fcheckreceive);
}

/*
 * Build Zmodem headers ...
 *
 * Note that we use 32 bit CRC's for ZHEX headers.
 *
 * This function is a combination of zm fns: zsbhdr(), zsbh32(), and zshhdr().
 */

static int
czbuild_header(zresult, ipkttype, ihdrtype, hdrval)
char *zresult;
int ipkttype;
int ihdrtype;
hdrval_t hdrval;
{
	char *p;
	int i;
	unsigned long crc;
	achdrval_t achdrval;

	p = zresult;

	switch (ipkttype) {
	case ZBIN:
		*p++ = ZPAD;
		*p++ = ZDLE;
		*p++ = ZBIN;
		p = zputchar (p, ihdrtype);
		crc = ICRCINIT;
		crc = UPDC32 (ihdrtype, crc);
		stohdr (hdrval, achdrval);
		for (i = 0; i < 4; i++) {
			p = zputchar (p, achdrval[i]);
			crc = UPDC32 (achdrval[i], crc);
		}
		crc = ~crc;
		for (i = 0; i < 4; i++) {
			p = zputchar (p, (char) crc);
			crc >>= 8;
		}
		break;
	case ZHEX: 	/* build hex header */
		*p++ = ZPAD;
		*p++ = ZPAD;
		*p++ = ZDLE;
		*p++ = ZHEX;
		p = zputhex (p, ihdrtype);
		crc = ICRCINIT;
		crc = UPDC32 (ihdrtype, crc);
		stohdr (hdrval, achdrval);
		for (i = 0; i < 4; i++) {
			p = zputhex (p, achdrval[i]);
			crc = UPDC32 (achdrval[i], crc);
		}
		crc = ~crc;
		for (i = 0; i < 4; i++) {
			p = zputhex (p, (char) crc);
			crc >>= 8;
		}
		*p++ = CR;
		/*
		 * Uncork the remote in case a fake XOFF has stopped data flow.
		 */
		if (ihdrtype != ZFIN && ihdrtype != ZACK) /* FIXME: why? */
			*p++ = XON;
		break;
	default:
		ulog (LOG_FATAL, "czbuild_header: ipkttype == %d", ipkttype);
		break;
	}

	return p - zresult;
}

/*
 * Build Zmodem data packets ...
 *
 * This function is zsdata() and zsda32() from the zm source.
 */

static int
czbuild_data_packet(zresult, zdata, cdata, frameend)
char *zresult;
const char *zdata;
size_t cdata;
int frameend;
{
	char *p;
	unsigned long crc;

	p = zresult;

	crc = ICRCINIT;
	for ( ; cdata-- != 0; zdata++) {
		char c;

		c = *zdata;
		if (c & 0140)
			*p++ = c;
		else
			p = zputchar (p, c);
		crc = UPDC32 ((unsigned char) c, crc);
	}
	*p++ = ZDLE;
	*p++ = frameend;
	crc = UPDC32 (frameend, crc);
	crc = ~crc;
	for (cdata = 0; cdata < 4; cdata++) {
		p = zputchar (p, (char) crc);
		crc >>= 8;
	}
	if (frameend == ZCRCW || frameend == ZCRCE || frameend == ZCRCF) {
		*p++ = CR;
		*p++ = XON;
	}

	return p - zresult;
}

/*
 * Read in a header ...
 *
 * This is function zgethdr() from the Zmodem source.
 */

static int
izrecv_hdr(qdaemon, hdr)
struct sdaemon *qdaemon;
achdrval_t hdr;
{
	int c,cerr;

	/*
	 * Kludge alert! If another part of the program received a packet but
	 * wasn't ready to handle it, it is tucked away for us to handle now.
	 */

	if (iZpkt_rcvd_kludge != -1) {
		c = iZpkt_rcvd_kludge;
		iZpkt_rcvd_kludge = -1;
		stohdr (hvZpkt_hdrval_kludge, hdr);
		DEBUG_MESSAGE2 (DEBUG_PROTO,
				"izrecv_hdr: queued %s, data = 0x%lx",
				ZZHEADER_NAME(c), rclhdr (hdr));
		cZheaders_received++;
		return c;
	}

	cerr = cZmax_garbage;	/* Max bytes before start of frame */

again:
	switch (c = noxrd7 (qdaemon)) {
	case ZM_TIMEOUT:
	case ZM_ERROR:
	case ZM_RCDO:
		goto fifi;
	case ZPAD:		/* This is what we want */
		break;
	case CR:		/* padding at end of previous header */
	default:
		if (--cerr < 0) {
			c = ZM_ERROR;
			goto fifi;
		}
		goto again;
	}

splat:
	switch (c = noxrd7 (qdaemon)) {
	case ZPAD:
		if (--cerr < 0) {
			c = ZM_ERROR;
			goto fifi;
		}
		goto splat;
	case ZM_TIMEOUT:
	case ZM_RCDO:
		goto fifi;
	case ZDLE:		/* This is what we want */
		break;
	default:
		if (--cerr < 0) {
			c = ZM_ERROR;
			goto fifi;
		}
		goto again;
	}

	switch (c = noxrd7 (qdaemon)) {
	case ZM_TIMEOUT:
	case ZM_RCDO:
		goto fifi;
	case ZBIN:
		c = zrbhdr32 (qdaemon, hdr);
		break;
	case ZHEX:
		c = zrhhdr (qdaemon, hdr);
		break;
	default:
		if (--cerr < 0) {
			c = ZM_ERROR;
			goto fifi;
		}
		goto again;
	}

fifi:
	switch (c) {
	case ZM_TIMEOUT:
		cZtimeouts++;
		break;
	case ZM_ERROR:
		cZerrors++;
		break;
	case ZM_RCDO:
		break;
	default:
		cZheaders_received++;
		break;
	}
	DEBUG_MESSAGE2 (DEBUG_PROTO, "izrecv_hdr: %s, data = 0x%lx",
			ZZHEADER_NAME(c), rclhdr (hdr));

	return c;
}

/*
 * Receive a binary style header (type and position) with 32 bit FCS ...
 */

static int
zrbhdr32(qdaemon, hdr)
struct sdaemon *qdaemon;
achdrval_t hdr;
{
	int c,i,type;
	unsigned long crc;

	if ((c = zdlread (qdaemon)) & ~0377)
		return c;
	type = c;
	crc = ICRCINIT;
	crc = UPDC32 (c, crc);

	for (i = 0; i < 4; i++) {
		if ((c = zdlread (qdaemon)) & ~0377)
			return c;
		crc = UPDC32 (c, crc);
		hdr[i] = (char) c;
	}
	for (i = 0; i < 4; i++) {
		if ((c = zdlread (qdaemon)) & ~0377)
			return c;
		crc = UPDC32 (c, crc);
	}
	if (crc != IHDRCRC)
		return ZM_ERROR;

	return type;
}

/*
 * Receive a hex style header (type and position) ...
 */

static int
zrhhdr(qdaemon, hdr)
struct sdaemon *qdaemon;
achdrval_t hdr;
{
	int c,i,type;
	unsigned long crc;

	if ((c = zgethex (qdaemon)) < 0)
		return c;
	type = c;
	crc = ICRCINIT;
	crc = UPDC32 (c, crc);

	for (i = 0; i < 4; i++) {
		if ((c = zgethex (qdaemon)) < 0)
			return c;
		crc = UPDC32 (c, crc);
		hdr[i] = (char) c;
	}
	for (i = 0; i < 4; i++) {
		if ((c = zgethex (qdaemon)) < 0)
			return c;
		crc = UPDC32 (c, crc);
	}
	if (crc != IHDRCRC)
		return ZM_ERROR;

	return type;
}

/*
 * Receive a data packet ...
 */

static int
zrdat32(qdaemon, buf, length, iprxcount)
struct sdaemon *qdaemon;
char *buf;
int length;
int *iprxcount;
{
	int c,d;
	unsigned long crc;
	char *end;

	crc = ICRCINIT;
	*iprxcount = 0;
	end = buf + length;
	while (buf <= end) {
		if ((c = zdlread (qdaemon)) & ~0377) {
crcfoo:
			switch (c) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
			case GOTCRCF:
				d = c;
				c &= 0377;
				crc = UPDC32 (c, crc);
				if ((c = zdlread (qdaemon)) & ~0377)
					goto crcfoo;
				crc = UPDC32 (c, crc);
				if ((c = zdlread (qdaemon)) & ~0377)
					goto crcfoo;
				crc = UPDC32 (c, crc);
				if ((c = zdlread (qdaemon)) & ~0377)
					goto crcfoo;
				crc = UPDC32 (c, crc);
				if ((c = zdlread (qdaemon)) & ~0377)
					goto crcfoo;
				crc = UPDC32 (c, crc);
				if (crc != IHDRCRC)
					return ZM_ERROR;
				*iprxcount = length - (end - buf);
				return d;
			case ZM_TIMEOUT:
			case ZM_RCDO:
				return c;
			default:
				return ZM_ERROR;
			}
		}
		*buf++ = (char) c;
		crc = UPDC32 (c, crc);
	}

	return ZM_ERROR;	/* bad packet, too long */
}

/*
 * Respond to receiver's complaint, get back in sync with receiver ...
 */

static int
getinsync(qdaemon, flag)
struct sdaemon *qdaemon;
boolean flag;
{
	int c,cerr;
	achdrval_t rx_hdr;

	cerr = cZretries;

	for (;;) {
		c = izrecv_hdr (qdaemon, rx_hdr);

		switch (c) {
		case ZRPOS:
			wpZrxpos = lzupdate_rxpos (rx_hdr, wpZrxpos,
						   wpZlrxpos, wpZtxpos);
			cZbytes_resent += wpZtxpos - wpZrxpos;
			wpZlrxpos = wpZtxpos = wpZrxpos;
			if (wpZlastsync == wpZrxpos) {
				if (++iZbeenhereb4 > 4)
					if (cZblklen > 32)
						cZblklen /= 2;
				/* FIXME: shouldn't we reset iZbeenhereb4? */
			}
			wpZlastsync = wpZrxpos;
			return ZRPOS;
		case ZACK:
			wpZrxpos = lzupdate_rxpos (rx_hdr, wpZrxpos,
						   wpZlrxpos, wpZtxpos);
			wpZlrxpos = wpZrxpos;
			if (flag || wpZtxpos == wpZrxpos)
				return ZACK;
			break;
		case ZNAK: {
			winpos_t rx_bytes;
			/*
			 * Our partner is in fzfinish_tx() and is waiting
			 * for ZACK ...
			 */
			zdecode_data_hdr (rclhdr (rx_hdr), &rx_bytes);
			if (rx_bytes == wpZrxbytes) {
				if (!fzsend_hdr (qdaemon, ZHEX, ZACK,
					     hvzencode_data_hdr (wpZrxbytes),
					     TRUE))
					return FALSE;
			}
			break;
		}
		case ZFIN:
		case ZM_RCDO:
			return c;
		case ZM_TIMEOUT:
			if (--cerr < 0) {
				ulog (LOG_ERROR,
				      "getinsync: retries exhausted");
				return ZM_ERROR;
			}
			break;	/* sender doesn't send ZNAK for timeout */
		case ZM_ERROR:
		default:
			if (--cerr < 0) {
				ulog (LOG_ERROR,
				      "getinsync: retries exhausted");
				return ZM_ERROR;
			}
			if (!fzsend_hdr (qdaemon, ZHEX, ZNAK,
					 hvzencode_data_hdr (wpZtxpos),
					 TRUE))
				return ZM_ERROR;
			break;
		}
	}
}

/*
 * Send a byte as two hex digits ...
 */

static char *
zputhex(p, ch)
char *p;
int ch;
{
	static char digits[] = "0123456789abcdef";

	*p++ = digits[(ch & 0xF0) >> 4];
	*p++ = digits[ch & 0xF];
	return p;
}

/*
 * Send character c with ZMODEM escape sequence encoding ...
 *
 * Escape XON, XOFF.
 * FIXME: Escape CR following @ (Telenet net escape) ... disabled for now
 *	Will need to put back references to <lastsent>.
 */

static char *
zputchar(p, ch)
char *p;
int ch;
{
	char c = ch;

	/* Quick check for non control characters */

	if (c & 0140) {
		*p++ = c;
	} else {
		switch (c & 0377) {
		case ZDLE:
			*p++ = ZDLE;
			*p++ = c ^ 0100;
			break;
		case CR:
#if 0
			if (!fZesc_ctl && (lastsent & 0177) != '@')
				goto sendit;
#endif
			/* fall through */
		case 020:	/* ^P */
		case XON:
		case XOFF:
			*p++ = ZDLE;
			c ^= 0100;
/*sendit:*/
			*p++ = c;
			break;
		default:
			if (fZesc_ctl && !(c & 0140)) {
				*p++ = ZDLE;
				c ^= 0100;
			}
			*p++ = c;
			break;
		}
	}

	return p;
}

/*
 * Decode two lower case hex digits into an 8 bit byte value ...
 */

static int
zgethex(qdaemon)
struct sdaemon *qdaemon;
{
	int c,n;

	if ((c = noxrd7 (qdaemon)) < 0)
		return c;
	n = c - '0';
	if (n > 9)
		n -= ('a' - ':');
	if (n & ~0xF)
		return ZM_ERROR;
	if ((c = noxrd7 (qdaemon)) < 0)
		return c;
	c -= '0';
	if (c > 9)
		c -= ('a' - ':');
	if (c & ~0xF)
		return ZM_ERROR;
	c += (n << 4);

	return c;
}

/*
 * Read a byte, checking for ZMODEM escape encoding ...
 */

static int
zdlread(qdaemon)
struct sdaemon *qdaemon;
{
	int c;

again:
	READCHAR (qdaemon, c, cZtimeout);
	if (c < 0)
		return c;
	if (c & 0140)		/* quick check for non control characters */
		return c;
	switch (c) {
	case ZDLE:
		break;
	case XON:
		goto again;
	case XOFF:
		READCHAR (qdaemon, c, XON_WAIT);
		goto again;
	default:
		if (fZesc_ctl && !(c & 0140))
			goto again;
		return c;
	}

again2:
	READCHAR (qdaemon, c, cZtimeout);
	if (c < 0)
		return c;
	switch (c) {
	case ZCRCE:
	case ZCRCG:
	case ZCRCQ:
	case ZCRCW:
	case ZCRCF:
		return c | GOTOR;
	case ZRUB0:			/* FIXME: This is never generated. */
		return 0177;
	case ZRUB1:			/* FIXME: This is never generated. */
		return 0377;
	case XON:
		goto again2;
	case XOFF:
		READCHAR (qdaemon, c, XON_WAIT);
		goto again2;
	default:
		if (fZesc_ctl && !(c & 0140))
			goto again2;		/* FIXME: why again2? */
		if ((c & 0140) == 0100)
			return c ^ 0100;
		break;
	}

	return ZM_ERROR;
}

/*
 * Read a character from the modem line with timeout ...
 * Eat parity bit, XON and XOFF characters.
 */

static int
noxrd7(qdaemon)
struct sdaemon *qdaemon;
{
	int c;

	for (;;) {
		READCHAR (qdaemon, c, cZtimeout);
		if (c < 0)
			return c;
		switch (c &= 0177) {
		case XON:
			continue;
		case XOFF:
			READCHAR (qdaemon, c, XON_WAIT);
			continue;
		case CR:
		case ZDLE:
			return c;
		default:
			if (fZesc_ctl && !(c & 0140))
				continue;
			return c;
		}
	}
}

/*
 * Read a character from the receive buffer, or from the line if empty ...
 *
 * <timeout> is in seconds (maybe make it tenths of seconds like in Zmodem?)
 */

static int
realreadchar(qdaemon, timeout)
struct sdaemon *qdaemon;
int timeout;
{
	int c;

	if ((c = breceive_char (qdaemon->qconn, timeout, TRUE)) >= 0)
		return c;

	switch (c) {
	case -1:
		return ZM_TIMEOUT;
	case -2:
		return ZM_RCDO;
	}

	ulog (LOG_FATAL, "realreadchar: breceive_char() returned %d", c);
	return ZM_ERROR;
}


/*
 * Check if the receive channel has any characters in it.
 *
 * At present we can only test the receive buffer. No mechanism is available
 * to go to the hardware. This should not be a problem though, as long as all
 * appropriate calls to fsend_data() set <fdoread> to TRUE.
 */

static boolean
fzreceive_ready()
{
	return iPrecstart != iPrecend;
}

/*
 * Store integer value in an achdrval_t ...
 */

static void
stohdr(val, hdr)
hdrval_t val;
achdrval_t hdr;
{
	hdr[ZP0] = (char) val;
	hdr[ZP1] = (char) (val >> 8);
	hdr[ZP2] = (char) (val >> 16);
	hdr[ZP3] = (char) (val >> 24);
}

/*
 * Recover an integer from a header ...
 */

static hdrval_t
rclhdr(hdr)
achdrval_t hdr;
{
	hdrval_t v;

	v = hdr[ZP3] & 0377;
	v = (v << 8) | (hdr[ZP2] & 0377);
	v = (v << 8) | (hdr[ZP1] & 0377);
	v = (v << 8) | (hdr[ZP0] & 0377);

	return v;
}

/*
 * Encode a <hdrval_t> from the byte count ...
 *
 * We use to store the byte count / 32 and a message sequence number which
 * made this function very useful. Don't remove it.
 * FIXME: Well, maybe remove it later.
 */

static hdrval_t
hvzencode_data_hdr(cbytes)
winpos_t cbytes;
{
	return (hdrval_t) cbytes;
}

/*
 * Decode a <hdrval_t> into a byte count ...
 *
 * We use to store the byte count / 32 and a message sequence number which
 * made this function very useful. Don't remove it.
 * FIXME: Well, maybe remove it later.
 */

static void
zdecode_data_hdr(hdrval, pcbytes)
hdrval_t hdrval;
winpos_t *pcbytes;
{
	*pcbytes = hdrval;
}

/*
 * Update <wpZrxpos> from the received data header value ...
 *
 * FIXME: Here is where we'd handle wrapping around at 4 gigabytes.
 */

static winpos_t
lzupdate_rxpos(rx_hdr, rxpos, lrxpos, txpos)
achdrval_t rx_hdr;
winpos_t rxpos,lrxpos,txpos;
{
	winpos_t rx_pktpos;

	zdecode_data_hdr (rclhdr (rx_hdr), &rx_pktpos);

	DEBUG_MESSAGE4 (DEBUG_PROTO,
   "lzupdate_rxpos: rx_pktpos=0x%lx, rxpos=0x%lx, lrxpos=0x%lx, txpos=0x%lx",
			rx_pktpos, rxpos, lrxpos, txpos);

	/*
	 * Check if <rx_pktpos> valid. It could be old.
	 */

	if (rx_pktpos < wpZlrxpos
	    || rx_pktpos > ((wpZtxpos + 1024L) & ~1023L))
		return rxpos;

	return rx_pktpos;
}
