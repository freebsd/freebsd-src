/*
 * ccp.c - PPP Compression Control Protocol.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 */

#ifndef lint
static char rcsid[] = "$Id: ccp.c,v 1.11 1995/08/16 04:15:38 paulus Exp $";
#endif

#include <syslog.h>
#include <sys/ioctl.h>
#include <net/ppp-comp.h>

#include "pppd.h"
#include "fsm.h"
#include "ccp.h"

fsm ccp_fsm[NUM_PPP];
ccp_options ccp_wantoptions[NUM_PPP];	/* what to request the peer to use */
ccp_options ccp_gotoptions[NUM_PPP];	/* what the peer agreed to do */
ccp_options ccp_allowoptions[NUM_PPP];	/* what we'll agree to do */
ccp_options ccp_hisoptions[NUM_PPP];	/* what we agreed to do */

/*
 * Callbacks for fsm code.
 */
static void ccp_resetci __P((fsm *));
static int  ccp_cilen __P((fsm *));
static void ccp_addci __P((fsm *, u_char *, int *));
static int  ccp_ackci __P((fsm *, u_char *, int));
static int  ccp_nakci __P((fsm *, u_char *, int));
static int  ccp_rejci __P((fsm *, u_char *, int));
static int  ccp_reqci __P((fsm *, u_char *, int *, int));
static void ccp_up __P((fsm *));
static void ccp_down __P((fsm *));
static int  ccp_extcode __P((fsm *, int, int, u_char *, int));
static void ccp_rack_timeout __P(());

static fsm_callbacks ccp_callbacks = {
    ccp_resetci,
    ccp_cilen,
    ccp_addci,
    ccp_ackci,
    ccp_nakci,
    ccp_rejci,
    ccp_reqci,
    ccp_up,
    ccp_down,
    NULL,
    NULL,
    NULL,
    NULL,
    ccp_extcode,
    "CCP"
};

/*
 * Do we want / did we get any compression?
 */
#define ANY_COMPRESS(opt)	((opt).bsd_compress)

/*
 * Local state (mainly for handling reset-reqs and reset-acks
 */
static int ccp_localstate[NUM_PPP];
#define RACK_PENDING	1	/* waiting for reset-ack */
#define RREQ_REPEAT	2	/* send another reset-req if no reset-ack */

#define RACKTIMEOUT	1	/* second */

/*
 * ccp_init - initialize CCP.
 */
void
ccp_init(unit)
    int unit;
{
    fsm *f = &ccp_fsm[unit];

    f->unit = unit;
    f->protocol = PPP_CCP;
    f->callbacks = &ccp_callbacks;
    fsm_init(f);

    memset(&ccp_wantoptions[unit],  0, sizeof(ccp_options));
    memset(&ccp_gotoptions[unit],   0, sizeof(ccp_options));
    memset(&ccp_allowoptions[unit], 0, sizeof(ccp_options));
    memset(&ccp_hisoptions[unit],   0, sizeof(ccp_options));

    ccp_wantoptions[0].bsd_compress = 1;
    ccp_wantoptions[0].bsd_bits = 12;	/* default value */

    ccp_allowoptions[0].bsd_compress = 1;
    ccp_allowoptions[0].bsd_bits = BSD_MAX_BITS;
}

/*
 * ccp_open - CCP is allowed to come up.
 */
void
ccp_open(unit)
    int unit;
{
    fsm *f = &ccp_fsm[unit];

    if (f->state != OPENED)
	ccp_flags_set(unit, 1, 0);
    if (!ANY_COMPRESS(ccp_wantoptions[unit]))
	f->flags |= OPT_SILENT;
    fsm_open(f);
}

/*
 * ccp_close - Terminate CCP.
 */
void
ccp_close(unit)
    int unit;
{
    ccp_flags_set(unit, 0, 0);
    fsm_close(&ccp_fsm[unit]);
}

/*
 * ccp_lowerup - we may now transmit CCP packets.
 */
void
ccp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ccp_fsm[unit]);
}

/*
 * ccp_lowerdown - we may not transmit CCP packets.
 */
void
ccp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ccp_fsm[unit]);
}

/*
 * ccp_input - process a received CCP packet.
 */
void
ccp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm *f = &ccp_fsm[unit];
    int oldstate;

    /*
     * Check for a terminate-request so we can print a message.
     */
    oldstate = f->state;
    fsm_input(f, p, len);
    if (oldstate == OPENED && p[0] == TERMREQ && f->state != OPENED)
	syslog(LOG_NOTICE, "Compression disabled by peer.");

    /*
     * If we get a terminate-ack and we're not asking for compression,
     * close CCP.
     */
    if (oldstate == REQSENT && p[0] == TERMACK
	&& !ANY_COMPRESS(ccp_gotoptions[unit]))
	ccp_close(unit);
}

/*
 * Handle a CCP-specific code.
 */
static int
ccp_extcode(f, code, id, p, len)
    fsm *f;
    int code, id;
    u_char *p;
    int len;
{
    switch (code) {
    case CCP_RESETREQ:
	if (f->state != OPENED)
	    break;
	/* send a reset-ack, which the transmitter will see and
	   reset its compression state. */
	fsm_sdata(f, CCP_RESETACK, id, NULL, 0);
	break;

    case CCP_RESETACK:
	if (ccp_localstate[f->unit] & RACK_PENDING && id == f->reqid) {
	    ccp_localstate[f->unit] &= ~(RACK_PENDING | RREQ_REPEAT);
	    UNTIMEOUT(ccp_rack_timeout, (caddr_t) f);
	}
	break;

    default:
	return 0;
    }

    return 1;
}

/*
 * ccp_protrej - peer doesn't talk CCP.
 */
void
ccp_protrej(unit)
    int unit;
{
    ccp_flags_set(unit, 0, 0);
    fsm_lowerdown(&ccp_fsm[unit]);
}

/*
 * ccp_resetci - initialize at start of negotiation.
 */
static void
ccp_resetci(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    u_char opt_buf[16];

    *go = ccp_wantoptions[f->unit];
    if (go->bsd_compress) {
	opt_buf[0] = CI_BSD_COMPRESS;
	opt_buf[1] = CILEN_BSD_COMPRESS;
	opt_buf[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits);
	if (!ccp_test(f->unit, opt_buf, CILEN_BSD_COMPRESS, 0))
	    go->bsd_compress = 0;
    }
}

/*
 * ccp_cilen - Return total length of our configuration info.
 */
static int
ccp_cilen(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];

    return (go->bsd_compress? CILEN_BSD_COMPRESS: 0);
}

/*
 * ccp_addci - put our requests in a packet.
 */
static void
ccp_addci(f, p, lenp)
    fsm *f;
    u_char *p;
    int *lenp;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    u_char *p0 = p;

    if (go->bsd_compress) {
	p[0] = CI_BSD_COMPRESS;
	p[1] = CILEN_BSD_COMPRESS;
	p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits);
	if (ccp_test(f->unit, p, CILEN_BSD_COMPRESS, 0))
	    p += CILEN_BSD_COMPRESS;
	else
	    go->bsd_compress = 0;
    }
    *lenp = p - p0;
}

/*
 * ccp_ackci - process a received configure-ack, and return
 * 1 iff the packet was OK.
 */
static int
ccp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ccp_options *go = &ccp_gotoptions[f->unit];

    if (go->bsd_compress) {
	if (len < CILEN_BSD_COMPRESS
	    || p[0] != CI_BSD_COMPRESS || p[1] != CILEN_BSD_COMPRESS
	    || p[2] != BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits))
	    return 0;
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
    }
    if (len != 0)
	return 0;
    return 1;
}

/*
 * ccp_nakci - process received configure-nak.
 * Returns 1 iff the nak was OK.
 */
static int
ccp_nakci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options no;		/* options we've seen already */
    ccp_options try;		/* options to ask for next time */

    memset(&no, 0, sizeof(no));
    try = *go;

    if (go->bsd_compress && !no.bsd_compress && len >= CILEN_BSD_COMPRESS
	&& p[0] == CI_BSD_COMPRESS && p[1] == CILEN_BSD_COMPRESS) {
	no.bsd_compress = 1;
	/*
	 * Peer wants us to use a different number of bits
	 * or a different version.
	 */
	if (BSD_VERSION(p[2]) != BSD_CURRENT_VERSION)
	    try.bsd_compress = 0;
	else if (BSD_NBITS(p[2]) < go->bsd_bits)
	    try.bsd_bits = BSD_NBITS(p[2]);
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
    }

    /*
     * Have a look at any remaining options...???
     */

    if (len != 0)
	return 0;

    if (f->state != OPENED)
	*go = try;
    return 1;
}

/*
 * ccp_rejci - reject some of our suggested compression methods.
 */
static int
ccp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options try;		/* options to request next time */

    try = *go;

    if (go->bsd_compress && len >= CILEN_BSD_COMPRESS
	&& p[0] == CI_BSD_COMPRESS && p[1] == CILEN_BSD_COMPRESS) {
	if (p[2] != BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits))
	    return 0;
	try.bsd_compress = 0;
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
    }

    if (len != 0)
	return 0;

    if (f->state != OPENED)
	*go = try;

    return 1;
}

/*
 * ccp_reqci - processed a received configure-request.
 * Returns CONFACK, CONFNAK or CONFREJ and the packet modified
 * appropriately.
 */
static int
ccp_reqci(f, p, lenp, dont_nak)
    fsm *f;
    u_char *p;
    int *lenp;
    int dont_nak;
{
    int ret, newret;
    u_char *p0, *retp;
    int len, clen, type, nb;
    ccp_options *ho = &ccp_hisoptions[f->unit];
    ccp_options *ao = &ccp_allowoptions[f->unit];

    ret = CONFACK;
    retp = p0 = p;
    len = *lenp;

    memset(ho, 0, sizeof(ccp_options));

    while (len > 0) {
	newret = CONFACK;
	if (len < 2 || p[1] < 2 || p[1] > len) {
	    /* length is bad */
	    clen = len;
	    newret = CONFREJ;

	} else {
	    type = p[0];
	    clen = p[1];

	    switch (type) {
	    case CI_BSD_COMPRESS:
		if (!ao->bsd_compress || clen != CILEN_BSD_COMPRESS) {
		    newret = CONFREJ;
		    break;
		}

		ho->bsd_compress = 1;
		ho->bsd_bits = nb = BSD_NBITS(p[2]);
		if (BSD_VERSION(p[2]) != BSD_CURRENT_VERSION
		    || nb > ao->bsd_bits) {
		    newret = CONFNAK;
		    nb = ao->bsd_bits;
		} else if (nb < BSD_MIN_BITS) {
		    newret = CONFREJ;
		} else if (!ccp_test(f->unit, p, CILEN_BSD_COMPRESS, 1)) {
		    if (nb > BSD_MIN_BITS) {
			--nb;
			newret = CONFNAK;
		    } else
			newret = CONFREJ;
		}
		if (newret == CONFNAK && !dont_nak) {
		    p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, nb);
		}

		break;

	    default:
		newret = CONFREJ;
	    }
	}

	if (newret == CONFNAK && dont_nak)
	    newret = CONFREJ;
	if (!(newret == CONFACK || newret == CONFNAK && ret == CONFREJ)) {
	    /* we're returning this option */
	    if (newret == CONFREJ && ret == CONFNAK)
		retp = p0;
	    ret = newret;
	    if (p != retp)
		BCOPY(p, retp, clen);
	    retp += clen;
	}

	p += clen;
	len -= clen;
    }

    if (ret != CONFACK)
	*lenp = retp - p0;
    return ret;
}

/*
 * CCP has come up - inform the kernel driver.
 */
static void
ccp_up(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options *ho = &ccp_hisoptions[f->unit];

    ccp_flags_set(f->unit, 1, 1);
    if (go->bsd_compress || ho->bsd_compress)
	syslog(LOG_NOTICE, "%s enabled",
	       go->bsd_compress? ho->bsd_compress? "Compression":
	       "Receive compression": "Transmit compression");
}

/*
 * CCP has gone down - inform the kernel driver.
 */
static void
ccp_down(f)
    fsm *f;
{
    if (ccp_localstate[f->unit] & RACK_PENDING)
	UNTIMEOUT(ccp_rack_timeout, (caddr_t) f);
    ccp_localstate[f->unit] = 0;
    ccp_flags_set(f->unit, 1, 0);
}

/*
 * Print the contents of a CCP packet.
 */
char *ccp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej",
    NULL, NULL, NULL, NULL, NULL, NULL,
    "ResetReq", "ResetAck",
};

int
ccp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    u_char *p0, *optend;
    int code, id, len;
    int optlen;

    p0 = p;
    if (plen < HEADERLEN)
	return 0;
    code = p[0];
    id = p[1];
    len = (p[2] << 8) + p[3];
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(ccp_codenames) / sizeof(char *)
	&& ccp_codenames[code-1] != NULL)
	printer(arg, " %s", ccp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;
    p += HEADERLEN;

    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	/* print list of possible compression methods */
	while (len >= 2) {
	    code = p[0];
	    optlen = p[1];
	    if (optlen < 2 || optlen > len)
		break;
	    printer(arg, " <");
	    len -= optlen;
	    optend = p + optlen;
	    switch (code) {
	    case CI_BSD_COMPRESS:
		if (optlen >= CILEN_BSD_COMPRESS) {
		    printer(arg, "bsd v%d %d", BSD_VERSION(p[2]),
			    BSD_NBITS(p[2]));
		    p += CILEN_BSD_COMPRESS;
		}
		break;
	    }
	    while (p < optend)
		printer(arg, " %.2x", *p++);
	    printer(arg, ">");
	}
	break;
    }

    /* dump out the rest of the packet in hex */
    while (--len >= 0)
	printer(arg, " %.2x", *p++);

    return p - p0;
}

/*
 * We have received a packet that the decompressor failed to
 * decompress.  Here we would expect to issue a reset-request, but
 * Motorola has a patent on resetting the compressor as a result of
 * detecting an error in the decompressed data after decompression.
 * (See US patent 5,130,993; international patent publication number
 * WO 91/10289; Australian patent 73296/91.)
 *
 * So we ask the kernel whether the error was detected after
 * decompression; if it was, we take CCP down, thus disabling
 * compression :-(, otherwise we issue the reset-request.
 */
void
ccp_datainput(unit, pkt, len)
    int unit;
    u_char *pkt;
    int len;
{
    fsm *f;

    f = &ccp_fsm[unit];
    if (f->state == OPENED) {
	if (ccp_fatal_error(unit)) {
	    /*
	     * Disable compression by taking CCP down.
	     */
	    syslog(LOG_ERR, "Lost compression sync: disabling compression");
	    ccp_close(unit);
	} else {
	    /*
	     * Send a reset-request to reset the peer's compressor.
	     * We don't do that if we are still waiting for an
	     * acknowledgement to a previous reset-request.
	     */
	    if (!(ccp_localstate[f->unit] & RACK_PENDING)) {
		fsm_sdata(f, CCP_RESETREQ, f->reqid = ++f->id, NULL, 0);
		TIMEOUT(ccp_rack_timeout, (caddr_t) f, RACKTIMEOUT);
		ccp_localstate[f->unit] |= RACK_PENDING;
	    } else
		ccp_localstate[f->unit] |= RREQ_REPEAT;
	}
    }
}

/*
 * Timeout waiting for reset-ack.
 */
static void
ccp_rack_timeout(arg)
    caddr_t arg;
{
    fsm *f = (fsm *) arg;

    if (f->state == OPENED && ccp_localstate[f->unit] & RREQ_REPEAT) {
	fsm_sdata(f, CCP_RESETREQ, f->reqid, NULL, 0);
	TIMEOUT(ccp_rack_timeout, (caddr_t) f, RACKTIMEOUT);
	ccp_localstate[f->unit] &= ~RREQ_REPEAT;
    } else
	ccp_localstate[f->unit] &= ~RACK_PENDING;
}

