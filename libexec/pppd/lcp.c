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

/*
 * TODO:
 * Keepalive.
 * Send NAKs for unsent CIs.
 * Keep separate MTU, MRU.
 * Option tracing.
 * Extra data on authtype option.
 * Test restart.
 */

#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>

#include <string.h>

#ifdef STREAMS
#include <sys/stream.h>
#include "ppp_str.h"
#endif

#include <net/if_ppp.h>
#include "pppd.h"

#include <net/ppp.h>
#include "fsm.h"
#include "lcp.h"
#include "magic.h"
#include "chap.h"
#include "upap.h"
#include "ipcp.h"

/* global vars */
fsm lcp_fsm[NPPP];		/* LCP fsm structure (global)*/
lcp_options lcp_wantoptions[NPPP]; /* Options that we want to request */
lcp_options lcp_gotoptions[NPPP]; /* Options that peer ack'd */
lcp_options lcp_allowoptions[NPPP]; /* Options that we allow peer to request */
lcp_options lcp_hisoptions[NPPP]; /* Options that we ack'd */

/* local vars */
static void lcp_resetci __ARGS((fsm *));
     /* Reset our Configuration Information */
static int lcp_cilen __ARGS((fsm *));	/* Return length of our CI */
static void lcp_addci __ARGS((fsm *, u_char *));	/* Add our CIs */
static int lcp_ackci __ARGS((fsm *, u_char *, int));	/* Ack some CIs */
static void lcp_nakci __ARGS((fsm *, u_char *, int)); /* Nak some CIs */
static void lcp_rejci __ARGS((fsm *, u_char *, int));
                                                 /* Reject some CIs */
static u_char lcp_reqci __ARGS((fsm *, u_char *, int *));
                                             /* Check the requested CIs */
static void lcp_up __ARGS((fsm *));			/* We're UP */
static void lcp_down __ARGS((fsm *));	/* We're DOWN */
static void lcp_closed __ARGS((fsm *));		/* We're CLOSED */

static fsm_callbacks lcp_callbacks = {	/* LCP callback routines */
    lcp_resetci,		/* Reset our Configuration Information */
    lcp_cilen,			/* Length of our Configuration Information */
    lcp_addci,			/* Add our Configuration Information */
    lcp_ackci,			/* ACK our Configuration Information */
    lcp_nakci,			/* NAK our Configuration Information */
    lcp_rejci,			/* Reject our Configuration Information */
    lcp_reqci,			/* Request peer's Configuration Information */
    lcp_up,			/* Called when fsm reaches OPEN state */
    lcp_down,			/* Called when fsm leaves OPEN state */
    lcp_closed,			/* Called when fsm reaches CLOSED state */
    NULL,			/* Called when Protocol-Reject received */
    NULL			/* Retransmission is necessary */
};




#define DEFWARNLOOPS	10	/* XXX Move to lcp.h */
static int lcp_warnloops = DEFWARNLOOPS; /* Warn about a loopback this often */


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
  f->timeouttime = DEFTIMEOUT;
  f->maxconfreqtransmits = DEFMAXCONFIGREQS;
  f->maxtermtransmits = DEFMAXTERMTRANSMITS;
  f->maxnakloops = DEFMAXNAKLOOPS;
  f->callbacks = &lcp_callbacks;
  
  wo->passive = 0;
  wo->restart = 0;			/* Set to 1 in kernels or multi-line
					   implementations */

  wo->neg_mru = 1;
  wo->mru = DEFMRU;
  wo->neg_asyncmap = 1;
  wo->asyncmap = 0;
  wo->neg_chap = 0;			/* Set to 1 on server */
  wo->neg_upap = 0;			/* Set to 1 on server */
  wo->neg_magicnumber = 1;
  wo->neg_pcompression = 1;
  wo->neg_accompression = 1;

  ao->neg_mru = 1;
  ao->neg_asyncmap = 1;
  ao->neg_chap = 0;			/* Set to 1 on client */
  ao->chap_mdtype = CHAP_DIGEST_MD5;
  ao->chap_callback = CHAP_NOCALLBACK;
  ao->neg_upap = 0;			/* Set to 1 on client */
  
  ao->neg_magicnumber = 1;
  ao->neg_pcompression = 1;
  ao->neg_accompression = 1;

  fsm_init(f);
}


/*
 * lcp_activeopen - Actively open LCP.
 */
void
  lcp_activeopen(unit)
int unit;
{
    fsm_activeopen(&lcp_fsm[unit]);
}


/*
 * lcp_passiveopen - Passively open LCP.
 */
void
  lcp_passiveopen(unit)
int unit;
{
    fsm_passiveopen(&lcp_fsm[unit]);
}


/*
 * lcp_close - Close LCP.
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
    SIFDOWN(unit);
    SIFMTU(unit, MTU);
    SIFASYNCMAP(unit, 0xffffffff);
    CIFPCOMPRESSION(unit);
    CIFACCOMPRESSION(unit);

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
	      "lcp_protrej: Received Protocol-Reject for LCP!"))
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
    /* this is marginal, as rejected-info should be full frame,
     * but at least we return the rejected-protocol
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
}


/*
 * lcp_cilen - Return length of our CI.
 */
static int
  lcp_cilen(f)
fsm *f;
{
    lcp_options *go = &lcp_gotoptions[f->unit];

#define LENCIVOID(neg) (neg ? 2 : 0)
#define LENCICHAP(neg) (neg ? 6 : 0)
#define LENCISHORT(neg)  (neg ? 4 : 0)
#define LENCILONG(neg)  (neg ? 6 : 0)

    return (LENCISHORT(go->neg_mru) +
	    LENCILONG(go->neg_asyncmap) +
	    LENCICHAP(go->neg_chap) +
	    LENCISHORT(go->neg_upap) +
	    LENCILONG(go->neg_magicnumber) +
	    LENCIVOID(go->neg_pcompression) +
	    LENCIVOID(go->neg_accompression));
}


/*
 * lcp_addci - Add our desired CIs to a packet.
 */
static void
  lcp_addci(f, ucp)
fsm *f;
u_char *ucp;
{
    lcp_options *go = &lcp_gotoptions[f->unit];

#define ADDCIVOID(opt, neg) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(2, ucp); \
    }
#define ADDCISHORT(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(2 + sizeof (short), ucp); \
	PUTSHORT(val, ucp); \
    }
#define ADDCICHAP(opt, neg, val, digest, callback) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(6, ucp); \
	PUTSHORT(val, ucp); \
	PUTCHAR(digest, ucp); \
	PUTCHAR(callback, ucp); \
    }
#define ADDCILONG(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(2 + sizeof (long), ucp); \
	PUTLONG(val, ucp); \
    }

    ADDCISHORT(CI_MRU, go->neg_mru, go->mru)
    ADDCILONG(CI_ASYNCMAP, go->neg_asyncmap, go->asyncmap)
    ADDCICHAP(CI_AUTHTYPE, go->neg_chap, CHAP, go->chap_mdtype, go->chap_callback)
    ADDCISHORT(CI_AUTHTYPE, go->neg_upap, UPAP)
    ADDCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber)
    ADDCIVOID(CI_PCOMPRESSION, go->neg_pcompression)
    ADDCIVOID(CI_ACCOMPRESSION, go->neg_accompression)
}


/*
 * lcp_ackci - Ack our CIs.
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
	if ((len -= 2) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 2 || \
	    citype != opt) \
	    goto bad; \
    }
#define ACKCISHORT(opt, neg, val) \
    if (neg) { \
	if ((len -= 2 + sizeof (short)) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 2 + sizeof (short) || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
    }
#define ACKCICHAP(opt, neg, val, digest, callback) \
    if (neg) { \
	if ((len -= 4 + sizeof (short)) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 4 + sizeof (short) || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != digest) \
	  goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != callback) \
	  goto bad; \
    }
#define ACKCILONG(opt, neg, val) \
    if (neg) { \
	if ((len -= 2 + sizeof (long)) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 2 + sizeof (long) || \
	    citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    goto bad; \
    }

    ACKCISHORT(CI_MRU, go->neg_mru, go->mru)
    ACKCILONG(CI_ASYNCMAP, go->neg_asyncmap, go->asyncmap)
    ACKCICHAP(CI_AUTHTYPE, go->neg_chap, CHAP, go->chap_mdtype, go->chap_callback)
    ACKCISHORT(CI_AUTHTYPE, go->neg_upap, UPAP)
    ACKCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber)
    ACKCIVOID(CI_PCOMPRESSION, go->neg_pcompression)
    ACKCIVOID(CI_ACCOMPRESSION, go->neg_accompression)

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);
bad:
    LCPDEBUG((LOG_WARNING, "lcp_acki: received bad Ack!"))
    return (0);
}


/*
 * lcp_nakci - NAK some of our CIs.
 */
static void
  lcp_nakci(f, p, len)
fsm *f;
u_char *p;
int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *wo = &lcp_wantoptions[f->unit];
    u_short cishort;
    u_long cilong;
    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIVOID(opt, neg, code) \
    if (neg && \
	len >= 2 && \
	p[1] == 2 && \
	p[0] == opt) { \
	len -= 2; \
	INCPTR(2, p); \
	code \
    }
#define NAKCICHAP(opt, neg, digest, callback, code) \
    if (neg && \
	len >= 4 + sizeof (short) && \
	p[1] == 4 + sizeof (short) && \
	p[0] == opt) { \
	len -= 4 + sizeof (short); \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	INCPTR(2, p); \
	code \
    }
#define NAKCISHORT(opt, neg, code) \
    if (neg && \
	len >= 2 + sizeof (short) && \
	p[1] == 2 + sizeof (short) && \
	p[0] == opt) { \
	len -= 2 + sizeof (short); \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	code \
    }
#define NAKCILONG(opt, neg, code) \
    if (neg && \
	len >= 2 + sizeof (long) && \
	p[1] == 2 + sizeof (long) && \
	p[0] == opt) { \
	len -= 2 + sizeof (long); \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	code \
    }

    /*
     * We don't care if they want to send us smaller packets than
     * we want.  Therefore, accept any MRU less than what we asked for,
     * but then ignore the new value when setting the MRU in the kernel.
     * If they send us a bigger MRU than what we asked, reject it and
     * let him decide to accept our value.
     */
    NAKCISHORT(CI_MRU, go->neg_mru,
	       if (cishort <= wo->mru)
		    go->mru = cishort;
	       else
		    goto bad;
	       )
    NAKCILONG(CI_ASYNCMAP, go->neg_asyncmap,
	      go->asyncmap |= cilong;
	      )
    NAKCICHAP(CI_AUTHTYPE, go->neg_chap, go->chap_mdtype, go->chap_callback,
       LCPDEBUG((LOG_WARNING, "Peer refuses to authenticate chap!"))
	       )
    NAKCISHORT(CI_AUTHTYPE, go->neg_upap,
       LCPDEBUG((LOG_WARNING, "Peer refuses to authenticate pap!"))
	       )
    NAKCILONG(CI_MAGICNUMBER, go->neg_magicnumber,
	      go->magicnumber = magic();
	      if (++go->numloops % lcp_warnloops == 0)
		  LCPDEBUG((LOG_INFO, "The line appears to be looped back."))
	      )
    NAKCIVOID(CI_PCOMPRESSION, go->neg_pcompression,
	       go->neg_pcompression = 0;
	      )
    NAKCIVOID(CI_ACCOMPRESSION, go->neg_accompression,
	       go->neg_accompression = 0;
	      )

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len == 0)
	return;
bad:
    LCPDEBUG((LOG_WARNING, "lcp_nakci: received bad Nak!"))
}


/*
 * lcp_rejci - Reject some of our CIs.
 */
static void
  lcp_rejci(f, p, len)
fsm *f;
u_char *p;
int len;
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    u_short cishort;
    u_long cilong;
    u_char *start = p;
    int myopt, myval, xval, plen = len;
    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIVOID(opt, neg) \
    myopt = opt; \
    if (neg && \
	len >= 2 && \
	p[1] == 2 && \
	p[0] == opt) { \
	len -= 2; \
	INCPTR(2, p); \
	neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected void opt %d",opt)) \
    }
#define REJCISHORT(opt, neg, val) \
    myopt = opt; myval = val; \
    if (neg && \
	len >= 2 + sizeof (short) && \
	p[1] == 2 + sizeof (short) && \
	p[0] == opt) { \
	len -= 2 + sizeof (short); \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */ \
  	xval = cishort; \
	if (cishort != val) \
	    goto bad; \
	neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected short opt %d", opt)) \
    }
#define REJCICHAP(opt, neg, val, digest, callback) \
    myopt = opt; myval = val; \
    if (neg && \
	len >= 4 + sizeof (short) && \
	p[1] == 4 + sizeof (short) && \
	p[0] == opt) { \
	len -= 4 + sizeof (short); \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */ \
  	xval = cishort; \
	if (cishort != val) \
	    goto bad; \
	neg = 0; \
	INCPTR(2, p); \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected chap opt %d", opt)) \
    }
#define REJCILONG(opt, neg, val) \
    myopt = opt; myval = val; \
    if (neg && \
	len >= 2 + sizeof (long) && \
	p[1] == 2 + sizeof (long) && \
	p[0] == opt) { \
	len -= 2 + sizeof (long); \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
        xval = cilong; \
	/* Check rejected value. */ \
	if (cilong != val) \
	    goto bad; \
	neg = 0; \
	LCPDEBUG((LOG_INFO,"lcp_rejci rejected long opt %d", opt)) \
    }

    REJCISHORT(CI_MRU, go->neg_mru, go->mru)
    REJCILONG(CI_ASYNCMAP, go->neg_asyncmap, go->asyncmap)
    REJCICHAP(CI_AUTHTYPE, go->neg_chap, CHAP, go->chap_mdtype, go->callback)
    REJCISHORT(CI_AUTHTYPE, go->neg_upap, UPAP)
    REJCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber)
    REJCIVOID(CI_PCOMPRESSION, go->neg_pcompression)
    REJCIVOID(CI_ACCOMPRESSION, go->neg_accompression)

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len == 0)
	return;
bad:
    LCPDEBUG((LOG_WARNING, "lcp_rejci: received bad Reject!"))
    LCPDEBUG((LOG_WARNING, "lcp_rejci: plen %d len %d off %d, exp opt %d, found %d, val %d fval %d ",
	plen, len, p - start, myopt, p[0] &0xff, myval, xval ))
}


/*
 * lcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.
 */
static u_char
  lcp_reqci(f, inp, len)
fsm *f;
u_char *inp;		/* Requested CIs */
int *len;			/* Length of requested CIs */
{
    lcp_options *go = &lcp_gotoptions[f->unit];
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *ao = &lcp_allowoptions[f->unit];
    u_char *cip;		/* Pointer to Current CI */
    u_char cilen, citype, cichar;/* Parsed len, type, char value */
    u_short cishort;		/* Parsed short value */
    u_long cilong;		/* Parse long value */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p = inp;		/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */

    /*
     * Reset all his options.
     */
    ho->neg_mru = 0;
    ho->neg_asyncmap = 0;
    ho->neg_chap = 0;
    ho->neg_upap = 0;
    ho->neg_magicnumber = 0;
    ho->neg_pcompression = 0;
    ho->neg_accompression = 0;

    /*
     * Process all his options.
     */
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    LCPDEBUG((LOG_WARNING, "lcp_reqci: bad CI length!"))
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	cilen -= 2;			/* Adjust cilen to just data */

	switch (citype) {		/* Check CI type */
	  case CI_MRU:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd MRU"))
	    if (!ao->neg_mru ||		/* Allow option? */
		cilen != sizeof (short)) { /* Check CI length */
		INCPTR(cilen, p); 	/* Skip rest of CI */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETSHORT(cishort, p);	/* Parse MRU */
	    LCPDEBUG((LOG_INFO, "(%d)", cishort))

	    /*
	     * He must be able to receive at least our minimum.
	     * No need to check a maximum.  If he sends a large number,
	     * we'll just ignore it.
	     */
	    if (cishort < MINMRU) {
		orc = CONFNAK;		/* Nak CI */
		DECPTR(sizeof (short), p); /* Backup */
		PUTSHORT(MINMRU, p);	/* Give him a hint */
		break;
	    }
	    ho->neg_mru = 1;		/* Remember he sent and MRU */
	    ho->mru = cishort;		/* And remember value */
	    break;

	  case CI_ASYNCMAP:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd ASYNCMAP"))
	    if (!ao->neg_asyncmap ||
		cilen != sizeof (long)) {
		INCPTR(cilen, p);
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);
	    LCPDEBUG((LOG_INFO, "(%lx)", cilong))

	    /* XXX Accept anything he says */
#if 0
	    /*
	     * Asyncmap must be OR of two maps.
	     */
	    if ((lcp_wantoptions[f->unit].neg_asyncmap &&
		 cilong != (lcp_wantoptions[f->unit].asyncmap | cilong)) ||
		(!lcp_wantoptions[f->unit].neg_asyncmap &&
		 cilong != 0xffffffff)) {
		orc = CONFNAK;
		DECPTR(sizeof (long), p);
		PUTLONG(lcp_wantoptions[f->unit].neg_asyncmap ?
			lcp_wantoptions[f->unit].asyncmap | cilong :
			0xffffffff, p);
		break;
	    }
#endif
	    ho->neg_asyncmap = 1;
	    ho->asyncmap = cilong;
	    break;

	  case CI_AUTHTYPE:
	    if (cilen < sizeof (short) ||
		(!ao->neg_upap && !ao->neg_chap)) {
	      LCPDEBUG((LOG_WARNING,
			"lcp_reqci: rcvd AUTHTYPE, rejecting ...!"))
	      INCPTR(cilen, p);
	      orc = CONFREJ;
	      break;
	    }
	    GETSHORT(cishort, p);
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd AUTHTYPE (%x)",
		      cishort))

	    /*
	     * Authtype must be UPAP or CHAP.
	     */
	    if (cishort == UPAP) {
	      INCPTR(cilen - sizeof (u_short), p);
	      if (!ao->neg_upap) { /* we don't want to do PAP */
		LCPDEBUG((LOG_INFO,
			"lcp_reqci: rcvd AUTHTYPE PAP, rejecting..."))
		orc = CONFREJ;
		break;
	      }
	      ho->neg_upap = 1;
	      break;
	    }
	    else if (cishort == CHAP) {
	      INCPTR(cilen - sizeof (u_short), p);
	      if (!ao->neg_chap) { /* we don't want to do CHAP */
		LCPDEBUG((LOG_INFO,
			"lcp_reqci: rcvd AUTHTYPE CHAP, rejecting..."))
		orc = CONFREJ;
		break;
	      }
	      GETCHAR(cichar, p); /* get digest type*/
	      if (cichar != ao->chap_mdtype) {
		DECPTR(sizeof (u_char), p);
		orc = CONFNAK;
		PUTCHAR(ao->chap_mdtype, p);
		INCPTR(cilen - sizeof(u_char), p);
		break;
	      }
	      ho->chap_mdtype = cichar; /* save md type */
	      GETCHAR(cichar, p); /* get callback type*/
	      if (cichar != ao->chap_callback) { /* we don't callback yet */
		DECPTR(sizeof (u_char), p);
		orc = CONFNAK;
		PUTCHAR(CHAP_NOCALLBACK, p);
		INCPTR(cilen - sizeof(u_char), p);
		break;
	      }
	      ho->chap_callback = cichar; /* save callback */
	      ho->neg_chap = 1;
	      break;
	    }
	    else {
	      DECPTR(sizeof (short), p);
	      orc = CONFNAK;
	      if (ao->neg_chap) {	/* We prefer CHAP */
		PUTSHORT(CHAP, p);
	      }
	      else
		if (ao->neg_upap) {
		PUTSHORT(CHAP, p);
	      }
	      else {
		syslog(LOG_ERR, "Coding botch in lcp_reqci authnak. This shouldn't happen.");
		exit(1);
	      }
	      INCPTR(cilen - sizeof (u_short), p);
	      break;
	    }


	  case CI_MAGICNUMBER:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd MAGICNUMBER"))
	    if (!ao->neg_magicnumber ||
		cilen != sizeof (long)) {
		INCPTR(cilen, p);
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);
	    LCPDEBUG((LOG_INFO, "(%lx)", cilong))

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
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd PCOMPRESSION"))
	    if (!ao->neg_pcompression ||
		cilen != 0) {
		INCPTR(cilen, p);
		orc = CONFREJ;
		break;
	    }
	    ho->neg_pcompression = 1;
	    break;

	  case CI_ACCOMPRESSION:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd ACCOMPRESSION"))
	    if (!ao->neg_accompression ||
		cilen != 0) {
		INCPTR(cilen, p);
		orc = CONFREJ;
		break;
	    }
	    ho->neg_accompression = 1;
	    break;

	  default:
	    LCPDEBUG((LOG_INFO, "lcp_reqci: rcvd unknown option %d",
		      citype))
	    INCPTR(cilen, p);
	    orc = CONFREJ;
	    break;
	}
	cilen += 2;			/* Adjust cilen whole CI */

endswitch:
	LCPDEBUG((LOG_INFO, " (%s)",
		  orc == CONFACK ? "ACK" : (orc == CONFNAK ? "NAK" : "REJ")))
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (rc == CONFREJ)		/* Rejecting prior CI? */
		continue;		/* Don't send this one */
	    if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		rc = CONFNAK;		/* Not anymore... */
		ucp = inp;		/* Backup */
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
     * XXX If we wanted to send additional NAKs (for unsent CIs), the
     * code would go here.  This must be done with care since it might
     * require a longer packet than we received.
     */

    *len = ucp - inp;			/* Compute output length */
    LCPDEBUG((LOG_INFO, "lcp_reqci: returning %s.",
	      rc == CONFACK ? "CONFACK" :
	      rc == CONFNAK ? "CONFNAK" : "CONFREJ"))
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
    lcp_options *ho = &lcp_hisoptions[f->unit];
    lcp_options *go = &lcp_gotoptions[f->unit];
    int auth = 0;

    if (ho->neg_mru)
	SIFMTU(f->unit, ho->mru);
    if (ho->neg_asyncmap)
	SIFASYNCMAP(f->unit, ho->asyncmap);
    if (ho->neg_pcompression)
	SIFPCOMPRESSION(f->unit);
    if (ho->neg_accompression)
	SIFACCOMPRESSION(f->unit);
    SIFUP(f->unit);		/* Bring the interface up (set IFF_UP) */
    ChapLowerUp(f->unit);	/* Enable CHAP */
    upap_lowerup(f->unit);	/* Enable UPAP */
    ipcp_lowerup(f->unit);	/* Enable IPCP */
    if (go->neg_chap) {
	ChapAuthPeer(f->unit);
	auth = 1;
    }
    if (ho->neg_chap) {
	ChapAuthWithPeer(f->unit);
	auth = 1;
    }
    if (go->neg_upap) {
	upap_authpeer(f->unit);
	auth = 1;
    }
    if (ho->neg_upap) {
	upap_authwithpeer(f->unit);
	auth = 1;
    }
    if (!auth)
	ipcp_activeopen(f->unit);
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
  SIFDOWN(f->unit);
  SIFMTU(f->unit, MTU);
  SIFASYNCMAP(f->unit, 0xffffffff);
  CIFPCOMPRESSION(f->unit);
  CIFACCOMPRESSION(f->unit);
  ChapLowerDown(f->unit);
  upap_lowerdown(f->unit);
}


/*
 * lcp_closed - LCP has CLOSED.
 *
 * Alert other protocols.
 */
static void
  lcp_closed(f)
fsm *f;
{
    if (lcp_wantoptions[f->unit].restart) {
	if (lcp_wantoptions[f->unit].passive)
	    lcp_passiveopen(f->unit);	/* Start protocol in passive mode */
	else
	    lcp_activeopen(f->unit);	/* Start protocol in active mode */
    }
    else {
	EXIT(f->unit);
    }
}
