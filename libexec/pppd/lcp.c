/*
 * lcp.c - PPP Link Control Protocol.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$Id: lcp.c,v 1.3 1994/03/30 09:38:14 jkh Exp $";
#endif

/*
 * TODO:
 * Option tracing.
 * Test restart.
 */

#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_ppp.h>
#include <netinet/in.h>

#include <string.h>

#include "pppd.h"
#include "ppp.h"
#include "fsm.h"
#include "lcp.h"
#include "magic.h"
#include "chap.h"
#include "upap.h"
#include "ipcp.h"

/* global vars */
fsm lcp_fsm[_NPPP];			/* LCP fsm structure (global)*/
lcp_options lcp_wantoptions[_NPPP];	/* Options that we want to request */
lcp_options lcp_gotoptions[_NPPP];	/* Options that peer ack'd */
lcp_options lcp_allowoptions[_NPPP];	/* Options we allow peer to request */
lcp_options lcp_hisoptions[_NPPP];	/* Options that we ack'd */

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void lcp_resetci __ARGS((fsm *));	/* Reset our CI */
static int  lcp_cilen __ARGS((fsm *));		/* Return length of our CI */
static void lcp_addci __ARGS((fsm *, u_char *, int *)); /* Add our CI to pkt */
static int  lcp_ackci __ARGS((fsm *, u_char *, int)); /* Peer ack'd our CI */
static int  lcp_nakci __ARGS((fsm *, u_char *, int)); /* Peer nak'd our CI */
static int  lcp_rejci __ARGS((fsm *, u_char *, int)); /* Peer rej'd our CI */
static int  lcp_reqci __ARGS((fsm *, u_char *, int *, int)); /* Rcv peer CI */
static void lcp_up __ARGS((fsm *));		/* We're UP */
static void lcp_down __ARGS((fsm *));		/* We're DOWN */
static void lcp_starting __ARGS((fsm *));	/* We need lower layer up */
static void lcp_finished __ARGS((fsm *));	/* We need lower layer down */
static int  lcp_extcode __ARGS((fsm *, int, int, u_char *, int));
static void lcp_rprotrej __ARGS((fsm *, u_char *, int));

static fsm_callbacks lcp_callbacks = {	/* LCP callback routines */
    lcp_resetci,		/* Reset our Configuration Information */
    lcp_cilen,			/* Length of our Configuration Information */
    lcp_addci,			/* Add our Configuration Information */
    lcp_ackci,			/* ACK our Configuration Information */
    lcp_nakci,			/* NAK our Configuration Information */
    lcp_rejci,			/* Reject our Configuration Information */
    lcp_reqci,			/* Request peer's Configuration Information */
    lcp_up,			/* Called when fsm reaches OPENED state */
    lcp_down,			/* Called when fsm leaves OPENED state */
    lcp_starting,		/* Called when we want the lower layer up */
    lcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    lcp_extcode,		/* Called to handle LCP-specific codes */
    "LCP"			/* String name of protocol */
};

int lcp_warnloops = DEFWARNLOOPS; /* Warn about a loopback this often */

/*
 * Length of each type of configuration option (in octets)
 */
#define CILEN_VOID	2
#define CILEN_SHORT	4	/* CILEN_VOID + sizeof(short) */
#define CILEN_CHAP	5	/* CILEN_VOID + sizeof(short) + 1 */
#define CILEN_LONG	6	/* CILEN_VOID + sizeof(long) */
#define CILEN_LQR	8	/* CILEN_VOID + sizeof(short) + sizeof(long) */

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")


/*
 * lcp_init - Initialize LCP.
 */
void
lcp_init(unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *ao = &lcp_allowoptions[unit];

    f->unit = unit;
    f->protocol = LCP;
    f->callbacks = &lcp_callbacks;

    fsm_init(f);

    wo->passive = 0;
    wo->silent = 0;
    wo->restart = 0;			/* Set to 1 in kernels or multi-line
					   implementations */
    wo->neg_mru = 1;
    wo->mru = DEFMRU;
    wo->neg_asyncmap = 1;
    wo->asyncmap = 0;
    wo->neg_chap = 0;			/* Set to 1 on server */
    wo->neg_upap = 0;			/* Set to 1 on server */
    wo->chap_mdtype = CHAP_DIGEST_MD5;
    wo->neg_magicnumber = 1;
    wo->neg_pcompression = 1;
    wo->neg_accompression = 1;
    wo->neg_lqr = 0;			/* no LQR implementation yet */

    ao->neg_mru = 1;
    ao->mru = MAXMRU;
    ao->neg_asyncmap = 1;
    ao->asyncmap = 0;
    ao->neg_chap = 1;
    ao->chap_mdtype = CHAP_DIGEST_MD5;
    ao->neg_upap = 1;
    ao->neg_magicnumber = 1;
    ao->neg_pcompression = 1;
    ao->neg_accompression = 1;
    ao->neg_lqr = 0;			/* no LQR implementation yet */

}


/*
 * lcp_open - LCP is allowed to come up.
 */
void
lcp_open(unit)
    int unit;
{
    fsm *f = &lcp_fsm[unit];
    lcp_options *wo = &lcp_wantoptions[unit];

    f->flags = 0;
    if (wo->passive)
	f->flags |= OPT_PASSIVE;
    if (wo->silent)
	f->flags |= OPT_SILENT;
    fsm_open(f);
}


/*
 * lcp_close - Take LCP down.
 */
void
lcp_close(unit)
    int unit;
{
    fsm_close(&lcp_fsm[unit]);
}


/*
 * lcp_lowerup - The lower layer is up.
 */
void
lcp_lowerup(unit)
    int unit;
{
    sifdown(unit);
    ppp_send_config(unit, MTU, 0xffffffff, 0, 0);
    ppp_recv_config(unit, MTU, 0, 0, 0);
    peer_mru[unit] = MTU;

    fsm_lowerup(&lcp_fsm[unit]);
}


/*
 * lcp_lowerdown - The lower layer is down.
 */
void
lcp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&lcp_fsm[unit]);
}


/*
 * lcp_input - Input LCP packet.
 */
void
lcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&lcp_fsm[unit], p, len);
}


/*
 * lcp_extcode - Handle a LCP-specific code.
 */
static int
lcp_extcode(f, code, id, inp, len)
    fsm *f;
    int code, id;
    u_char *inp;
    int len;
{
    switch( code ){
    case PROTREJ:
	lcp_rprotrej(f, inp, len);
	break;
    
    case ECHOREQ:
	if( f->state != OPENED )
	    break;
	LCPDEBUG((LOG_INFO, "lcp: Echo-Request, Rcvd id %d", id));
	fsm_sdata(f, ECHOREP, id, inp, len);
	break;
    
    case ECHOREP:
    case DISCREQ:
	break;

    default:
	return 0;
    }
    return 1;
}

    
/*
 * lcp_rprotrej - Receive an Protocol-Reject.
 *
 * Figure out which protocol is rejected and inform it.
 */
static void
lcp_rprotrej(f, inp, len)
    fsm *f;
    u_char *inp;
    int len;
{
    u_short prot;

    LCPDEBUG((LOG_INFO, "lcp_rprotrej."));

    if (len < sizeof (u_short)) {
	LCPDEBUG((LOG_INFO,
		  "lcp_rprotrej: Rcvd short Protocol-Reject packet!"));
	return;
    }

    GETSHORT(prot, inp);

    LCPDEBUG((LOG_INFO,
	      "lcp_rprotrej: Rcvd Protocol-Reject packet for %x!",
	      prot));

    /*
     * Protocol-Reject packets received in any state other than the LCP
     * OPENED state SHOULD be silently discarded.
     */
    if( f->state != OPENED ){
	LCPDEBUG((LOG_INFO, "Protocol-Reject discarded: LCP in state %d",
		  f->state));
	return;
    }

    DEMUXPROTREJ(f->unit, prot);	/* Inform protocol */
}


/*
 * lcp_protrej - A Protocol-Reject was received.
 */
/*ARGSUSED*/
void
lcp_protrej(unit)
    int unit;
{
    /*
     * Can't reject LCP!
     */
    LCPDEBUG((LOG_WARNING,
	      "lcp_protrej: Received Protocol-Reject for LCP!"));
    fsm_protreject(&lcp_fsm[unit]);
}


/*
 * lcp_sprotrej - Send a Protocol-Reject for some protocol.
 */
void
lcp_sprotrej(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    /*
     * Send back the protocol and the information field of the
     * rejected packet.  We only get here if LCP is in the OPENED state.
     */
    p += 2;
    len -= 2;

    fsm_sdata(&lcp_fsm[unit], PROTREJ, ++lcp_fsm[unit].id,
	      p, len);
}


/*
 * lcp_resetci - Reset our CI.
 */
static void
  lcp_resetci(f)
fsm *f;
{
    lcp_wantoptions[f->unit].magicnumber = magic();
    lcp_wantoptions[f->unit].numloops = 0;
    lcp_gotoptions[f->unit] = lcp_wantoptions[f->unit];
    peer_mru[f->unit] = MTU;
}


/*
 * lcp_cilen - Return length of our CI.
 */
static int
lcp_cilen(f)
    fsm *f;
{
    lcp_options *go = &lcp_gotoptions[f->unit];

#define LENCIVOID(neg)	(neg ? CILEN_VOID : 0)
#define LENCICHAP(neg)	(neg ? CILEN_CHAP : 0)
#define LENCISHORT(neg)	(neg ? CILEN_SHORT : 0)
#define LENCILONG(neg)	(neg ? CILEN_LONG : 0)
#define LENCILQR(neg)	(neg ? CILEN_LQR: 0)
    /*
     * NB: we only ask for one of CHAP and UPAP, even if we will
     * accept either.
     */
    return (LENCISHORT(go->neg_mru) +
	    LENCILONG(go->neg_asyncmap) +
	    LENCICHAP(go->neg_chap) +
	    LENCISHORT(!go->neg_chap && go->neg_upap) +
	    LENCILQR(go->neg_lqr) +
	    LENCILONG(go->neg_magicnumber) +
	    LENCIVOID(go->neg_pcompression) +
	    LENCIVOID(go->neg_accompression));
}


/*
 * lcp_addci - Add our desired CIs to a packet.
 */
static void
lcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char *start_ucp = ucp;

#define ADDCIVOID(opt, neg) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_VOID, ucp); \
    }
#define ADDCISHORT(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_SHORT, ucp); \
	PUTSHORT(val, ucp); \
    }
#define ADDCICHAP(opt, neg, val, digest) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_CHAP, ucp); \
	PUTSHORT(val, ucp); \
	PUTCHAR(digest, ucp); \
    }
#define ADDCILONG(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LONG, ucp); \
	PUTLONG(val, ucp); \
    }
#define ADDCILQR(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LQR, ucp); \
	PUTSHORT(LQR, ucp); \
	PUTLONG(val, ucp); \
    }

    ADDCISHORT(CI_MRU, go->neg_mru, go->mru);
    ADDCILONG(CI_ASYNCMAP, go->neg_asyncmap, go->asyncmap);
    ADDCICHAP(CI_AUTHTYPE, go->neg_chap, CHAP, go->chap_mdtype);
    ADDCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, UPAP);
    ADDCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ADDCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ADDCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ADDCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

    if (ucp - start_ucp != *lenp) {
	/* this should never happen, because peer_mtu should be 1500 */
	syslog(LOG_ERR, "Bug in lcp_addci: wrong length");
    }
}


/*
 * lcp_ackci - Ack our CIs.
 * This should not modify any state if the Ack is bad.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
lcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char cilen, citype, cichar;
    u_short cishort;
    u_long cilong;

    /*
     * CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define ACKCIVOID(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_VOID) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_VOID || \
	    citype != opt) \
	    goto bad; \
    }
#define ACKCISHORT(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_SHORT) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_SHORT || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
    }
#define ACKCICHAP(opt, neg, val, digest) \
    if (neg) { \
	if ((len -= CILEN_CHAP) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAP || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != digest) \
	  goto bad; \
    }
#define ACKCILONG(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LONG) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LONG || \
	    citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    goto bad; \
    }
#define ACKCILQR(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LQR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LQR || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != LQR) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	  goto bad; \
    }

    ACKCISHORT(CI_MRU, go->neg_mru, go->mru);
    ACKCILONG(CI_ASYNCMAP, go->neg_asyncmap, go->asyncmap);
    ACKCICHAP(CI_AUTHTYPE, go->neg_chap, CHAP, go->chap_mdtype);
    ACKCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, UPAP);
    ACKCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ACKCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ACKCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ACKCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);
bad:
    LCPDEBUG((LOG_WARNING, "lcp_acki: received bad Ack!"));
    return (0);
}


/*
 * lcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if LCP is in the OPENED state.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static int
lcp_nakci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *wo = &lcp_wantoptions[f->unit];
    u_char cilen, citype, cichar, *next;
    u_short cishort;
    u_long cilong;
    lcp_options no;		/* options we've seen Naks for */
    lcp_options try;		/* options to request next time */
    int looped_back = 0;

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIVOID(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	no.neg = 1; \
	code \
    }
#define NAKCICHAP(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	no.neg = 1; \
	code \
    }
#define NAKCISHORT(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILONG(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILQR(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }

    /*
     * We don't care if they want to send us smaller packets than
     * we want.  Therefore, accept any MRU less than what we asked for,
     * but then ignore the new value when setting the MRU in the kernel.
     * If they send us a bigger MRU than what we asked, accept it, up to
     * the limit of the default MRU we'd get if we didn't negotiate.
     */
    NAKCISHORT(CI_MRU, neg_mru,
	       if (cishort <= wo->mru || cishort < DEFMRU)
		   try.mru = cishort;
	       );
    /*
     * Add any characters they want to our (receive-side) asyncmap.
     */
    NAKCILONG(CI_ASYNCMAP, neg_asyncmap,
	      try.asyncmap = go->asyncmap | cilong;
	      );
    /*
     * If they can't cope with our CHAP hash algorithm, we'll have
     * to stop asking for CHAP.  We haven't got any other algorithm.
     */
    NAKCICHAP(CI_AUTHTYPE, neg_chap,
	      try.neg_chap = 0;
	      );
    /*
     * Peer shouldn't send Nak for UPAP, protocol compression or
     * address/control compression requests; they should send
     * a Reject instead.  If they send a Nak, treat it as a Reject.
     */
    if (!go->neg_chap ){
	NAKCISHORT(CI_AUTHTYPE, neg_upap,
		   try.neg_upap = 0;
		   );
    }
    /*
     * If they can't cope with our link quality protocol, we'll have
     * to stop asking for LQR.  We haven't got any other protocol.
     * If they Nak the reporting period, take their value XXX ?
     */
    NAKCILONG(CI_QUALITY, neg_lqr,
	      if (cishort != LQR)
		  try.neg_lqr = 0;
	      else
	          try.lqr_period = cilong;
	      );
    /*
     * Check for a looped-back line.
     */
    NAKCILONG(CI_MAGICNUMBER, neg_magicnumber,
	      try.magicnumber = magic();
	      ++try.numloops;
	      looped_back = 1;
	      );

    NAKCIVOID(CI_PCOMPRESSION, neg_pcompression,
	      try.neg_pcompression = 0;
	      );
    NAKCIVOID(CI_ACCOMPRESSION, neg_accompression,
	      try.neg_accompression = 0;
	      );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If we see an option that we requested, or one we've already seen
     * in this packet, then this packet is bad.
     * If we wanted to respond by starting to negotiate on the requested
     * option(s), we could, but we don't, because except for the
     * authentication type and quality protocol, if we are not negotiating
     * an option, it is because we were told not to.
     * For the authentication type, the Nak from the peer means
     * `let me authenticate myself with you' which is a bit pointless.
     * For the quality protocol, the Nak means `ask me to send you quality
     * reports', but if we didn't ask for them, we don't want them.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if( (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_MRU:
	    if (go->neg_mru || no.neg_mru || cilen != CILEN_SHORT)
		goto bad;
	    break;
	case CI_ASYNCMAP:
	    if (go->neg_asyncmap || no.neg_asyncmap || cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_AUTHTYPE:
	    if (go->neg_chap || no.neg_chap || go->neg_upap || no.neg_upap)
		goto bad;
	    break;
	case CI_MAGICNUMBER:
	    if (go->neg_magicnumber || no.neg_magicnumber ||
		cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_PCOMPRESSION:
	    if (go->neg_pcompression || no.neg_pcompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_ACCOMPRESSION:
	    if (go->neg_accompression || no.neg_accompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_QUALITY:
	    if (go->neg_lqr || no.neg_lqr || cilen != CILEN_LQR)
		goto bad;
	    break;
	default:
	    goto bad;
	}
	p = next;
    }

    /* If there is still anything left, this packet is bad. */
    if (len != 0)
	goto bad;

    /*
     * OK, the Nak is good.  Now we can update state.
     */
    if (f->state != OPENED) {
	*go = try;
	if (looped_back && try.numloops % lcp_warnloops == 0)
	    LCPDEBUG((LOG_INFO, "The line appears to be looped back."));
    }

    return 1;

bad:
    LCPDEBUG((LOG_WARNING, "lcp_nakci: received bad Nak!"));
    return 0;
}


/*
 * lcp_rejci - Peer has Rejected some of our CIs.
 * This should not modify any state if the Reject is bad
 * or if LCP is in the OPENED state.
 *
 * Returns:
 *	0 - Reject was bad.
 *	1 - Reject was good.
 */
static int
lcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_char cichar;
    u_short cishort;
    u_long cilong;
    u_char *start = p;
    int plen = len;
    lcp_options try;		/* options to request next time */

    try = *go;

    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIVOID(opt, neg) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	try.neg = 0; \
	LCPDEBUG((LOG_INFO, "lcp_rejci rejected void opt %d", opt)); \
    }
#define REJCISHORT(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */ \
	if (cishort != val) \
	    goto bad; \
	try.neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected short opt %d", opt)); \
    }
#define REJCICHAP(opt, neg, val, digest) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cishort != val || cichar != digest) \
	    goto bad; \
	try.neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected chap opt %d", opt)); \
    }
#define REJCILONG(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	/* Check rejected value. */ \
	if (cilong != val) \
	    goto bad; \
	try.neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected long opt %d", opt)); \
    }
#define REJCILQR(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	/* Check rejected value. */ \
	if (cishort != LQR || cichar != val) \
	    goto bad; \
	try.neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected LQR opt %d", opt)); \
    }

    REJCISHORT(CI_MRU, neg_mru, go->mru);
    REJCILONG(CI_ASYNCMAP, neg_asyncmap, go->asyncmap);
    REJCICHAP(CI_AUTHTYPE, neg_chap, CHAP, go->chap_mdtype);
    if (!go->neg_chap) {
	REJCISHORT(CI_AUTHTYPE, neg_upap, UPAP);
    }
    REJCILQR(CI_QUALITY, neg_lqr, go->lqr_period);
    REJCILONG(CI_MAGICNUMBER, neg_magicnumber, go->magicnumber);
    REJCIVOID(CI_PCOMPRESSION, neg_pcompression);
    REJCIVOID(CI_ACCOMPRESSION, neg_accompression);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    /*
     * Now we can update state.
     */
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    LCPDEBUG((LOG_WARNING, "lcp_rejci: received bad Reject!"));
    LCPDEBUG((LOG_WARNING, "lcp_rejci: plen %d len %d off %d",
	      plen, len, p - start));
    return 0;
}


/*
 * lcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
lcp_reqci(f, inp, lenp, reject_if_disagree)
    fsm *f;
    u_char *inp;		/* Requested CIs */
    int *lenp;			/* Length of requested CIs */
    int reject_if_disagree;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_char cilen, citype, cichar;/* Parsed len, type, char value */
    u_short cishort;		/* Parsed short value */
    u_long cilong;		/* Parse long value */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *lenp;		/* Length left */

    /*
     * Reset all his options.
     */
    BZERO(ho, sizeof(*ho));

    /*
     * Process all his options.
     */
    next = inp;
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p = next;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    LCPDEBUG((LOG_WARNING, "lcp_reqci: bad CI length!"));
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	next += cilen;			/* Step to next CI */

	switch (citype) {		/* Check CI type */
	case CI_MRU:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd MRU"));
	    if (!ao->neg_mru ||		/* Allow option? */
		cilen != CILEN_SHORT) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETSHORT(cishort, p);	/* Parse MRU */
	    LCPDEBUG((LOG_INFO, "(%d)", cishort));

	    /*
	     * He must be able to receive at least our minimum.
	     * No need to check a maximum.  If he sends a large number,
	     * we'll just ignore it.
	     */
	    if (cishort < MINMRU) {
		orc = CONFNAK;		/* Nak CI */
		if( !reject_if_disagree ){
		    DECPTR(sizeof (short), p);	/* Backup */
		    PUTSHORT(MINMRU, p);	/* Give him a hint */
		}
		break;
	    }
	    ho->neg_mru = 1;		/* Remember he sent MRU */
	    ho->mru = cishort;		/* And remember value */
	    break;

	case CI_ASYNCMAP:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd ASYNCMAP"));
	    if (!ao->neg_asyncmap ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);
	    LCPDEBUG((LOG_INFO, "(%lx)", cilong));

	    /*
	     * Asyncmap must have set at least the bits
	     * which are set in lcp_allowoptions[unit].asyncmap.
	     */
	    if ((ao->asyncmap & ~cilong) != 0) {
		orc = CONFNAK;
		if( !reject_if_disagree ){
		    DECPTR(sizeof (long), p);
		    PUTLONG(ao->asyncmap | cilong, p);
		}
		break;
	    }
	    ho->neg_asyncmap = 1;
	    ho->asyncmap = cilong;
	    break;

	case CI_AUTHTYPE:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd AUTHTYPE"));
	    if (cilen < CILEN_SHORT ||
		!(ao->neg_upap || ao->neg_chap)) {
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);
	    LCPDEBUG((LOG_INFO, "(%x)", cishort));

	    /*
	     * Authtype must be UPAP or CHAP.
	     *
	     * Note: if both ao->neg_upap and ao->neg_chap are set,
	     * and the peer sends a Configure-Request with two
	     * authenticate-protocol requests, one for CHAP and one
	     * for UPAP, then we will reject the second request.
	     * Whether we end up doing CHAP or UPAP depends then on
	     * the ordering of the CIs in the peer's Configure-Request.
	     */

	    if (cishort == UPAP) {
		if (!ao->neg_upap ||	/* we don't want to do PAP */
		    ho->neg_chap ||	/* or we've already accepted CHAP */
		    cilen != CILEN_SHORT) {
		    LCPDEBUG((LOG_WARNING,
			      "lcp_reqci: rcvd AUTHTYPE PAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		ho->neg_upap = 1;
		break;
	    }
	    if (cishort == CHAP) {
		if (!ao->neg_chap ||	/* we don't want to do CHAP */
		    ho->neg_upap ||	/* or we've already accepted UPAP */
		    cilen != CILEN_CHAP) {
		    LCPDEBUG((LOG_INFO,
			      "lcp_reqci: rcvd AUTHTYPE CHAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		GETCHAR(cichar, p);	/* get digest type*/
		if (cichar != ao->chap_mdtype) {
		    orc = CONFNAK;
		    if( !reject_if_disagree ){
			DECPTR(sizeof (u_char), p);
			PUTCHAR(ao->chap_mdtype, p);
		    }
		    break;
		}
		ho->chap_mdtype = cichar; /* save md type */
		ho->neg_chap = 1;
		break;
	    }

	    /*
	     * We don't recognize the protocol they're asking for.
	     * Reject it.
	     */
	    orc = CONFREJ;
	    break;

	case CI_QUALITY:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd QUALITY"));
	    if (!ao->neg_lqr ||
		cilen != CILEN_LQR) {
		orc = CONFREJ;
		break;
	    }

	    GETSHORT(cishort, p);
	    GETLONG(cilong, p);
	    LCPDEBUG((LOG_INFO, "(%x %lx)", cishort, cilong));
	    if (cishort != LQR) {
		orc = CONFREJ;
		break;
	    }

	    /*
	     * Check the reporting period.
	     * XXX When should we Nak this, and what with?
	     */
	    break;

	case CI_MAGICNUMBER:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd MAGICNUMBER"));
	    if (!(ao->neg_magicnumber || go->neg_magicnumber) ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);
	    LCPDEBUG((LOG_INFO, "(%lx)", cilong));

	    /*
	     * He must have a different magic number.
	     */
	    if (go->neg_magicnumber &&
		cilong == go->magicnumber) {
		orc = CONFNAK;
		DECPTR(sizeof (long), p);
		cilong = magic();	/* Don't put magic() inside macro! */
		PUTLONG(cilong, p);
		break;
	    }
	    ho->neg_magicnumber = 1;
	    ho->magicnumber = cilong;
	    break;


	case CI_PCOMPRESSION:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd PCOMPRESSION"));
	    if (!ao->neg_pcompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_pcompression = 1;
	    break;

	case CI_ACCOMPRESSION:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd ACCOMPRESSION"));
	    if (!ao->neg_accompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_accompression = 1;
	    break;

	default:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd unknown option %d",
		      citype));
	    orc = CONFREJ;
	    break;
	}

endswitch:
	LCPDEBUG((LOG_INFO, " (%s)", CODENAME(orc)));
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (reject_if_disagree)	/* Getting fed up with sending NAKs? */
		orc = CONFREJ;		/* Get tough if so */
	    else {
		if (rc == CONFREJ)	/* Rejecting prior CI? */
		    continue;		/* Don't send this one */
		if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		    rc = CONFNAK;	/* Not anymore... */
		    ucp = inp;		/* Backup */
		}
	    }
	}
	if (orc == CONFREJ &&		/* Reject this CI */
	    rc != CONFREJ) {		/*  but no prior ones? */
	    rc = CONFREJ;
	    ucp = inp;			/* Backup */
	}
	if (ucp != cip)			/* Need to move CI? */
	    BCOPY(cip, ucp, cilen);	/* Move it */
	INCPTR(cilen, ucp);		/* Update output pointer */
    }

    /*
     * If we wanted to send additional NAKs (for unsent CIs), the
     * code would go here.  This must be done with care since it might
     * require a longer packet than we received.  At present there
     * are no cases where we want to ask the peer to negotiate an option.
     */

    *lenp = ucp - inp;			/* Compute output length */
    LCPDEBUG((LOG_INFO, "lcp_reqci: returning CONF%s.", CODENAME(rc)));
    return (rc);			/* Return final code */
}


/*
 * lcp_up - LCP has come UP.
 *
 * Start UPAP, IPCP, etc.
 */
static void
lcp_up(f)
    fsm *f;
{
    lcp_options *wo = &lcp_wantoptions[f->unit];
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];

    /*
     * Set our MTU to the smaller of the MTU we wanted and
     * the MRU our peer wanted.  If we negotiated an MRU,
     * set our MRU to the larger of value we wanted and
     * the value we got in the negotiation.
     */
    ppp_send_config(f->unit, (ho->neg_mru? MIN(ao->mru, ho->mru): MTU),
		    (ho->neg_asyncmap? ho->asyncmap: 0xffffffff),
		    ho->neg_pcompression, ho->neg_accompression);
    ppp_recv_config(f->unit, (go->neg_mru? MAX(wo->mru, go->mru): MTU),
		    (go->neg_asyncmap? go->asyncmap: 0xffffffff),
		    go->neg_pcompression, go->neg_accompression);

    if (ho->neg_mru)
	peer_mru[f->unit] = ho->mru;

    ChapLowerUp(f->unit);	/* Enable CHAP */
    upap_lowerup(f->unit);	/* Enable UPAP */
    ipcp_lowerup(f->unit);	/* Enable IPCP */

    link_established(f->unit);
}


/*
 * lcp_down - LCP has gone DOWN.
 *
 * Alert other protocols.
 */
static void
lcp_down(f)
    fsm *f;
{
    ipcp_lowerdown(f->unit);
    ChapLowerDown(f->unit);
    upap_lowerdown(f->unit);

    sifdown(f->unit);
    ppp_send_config(f->unit, MTU, 0xffffffff, 0, 0);
    ppp_recv_config(f->unit, MTU, 0, 0, 0);
    peer_mru[f->unit] = MTU;
    syslog(LOG_NOTICE, "Connection terminated.");
}


/*
 * lcp_starting - LCP needs the lower layer up.
 */
static void
lcp_starting(f)
    fsm *f;
{
    link_required(f->unit);
}


/*
 * lcp_finished - LCP has finished with the lower layer.
 */
static void
lcp_finished(f)
    fsm *f;
{
    link_terminated(f->unit);
}

