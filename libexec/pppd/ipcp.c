/*
 * ipcp.c - PPP IP Control Protocol.
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
static char rcsid[] = "$Id: ipcp.c,v 1.3 1994/03/30 09:38:13 jkh Exp $";
#endif

/*
 * TODO:
 */

#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_ppp.h>
#include <net/route.h>
#include <netinet/in.h>

#include <string.h>

#include "pppd.h"
#include "ppp.h"
#include "fsm.h"
#include "ipcp.h"


/* global vars */
ipcp_options ipcp_wantoptions[_NPPP];	/* Options that we want to request */
ipcp_options ipcp_gotoptions[_NPPP];	/* Options that peer ack'd */
ipcp_options ipcp_allowoptions[_NPPP];	/* Options we allow peer to request */
ipcp_options ipcp_hisoptions[_NPPP];	/* Options that we ack'd */

/* local vars */
static int cis_received[_NPPP];		/* # Conf-Reqs received */

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void ipcp_resetci __ARGS((fsm *));	/* Reset our CI */
static int  ipcp_cilen __ARGS((fsm *));	        /* Return length of our CI */
static void ipcp_addci __ARGS((fsm *, u_char *, int *)); /* Add our CI */
static int  ipcp_ackci __ARGS((fsm *, u_char *, int));	/* Peer ack'd our CI */
static int  ipcp_nakci __ARGS((fsm *, u_char *, int));	/* Peer nak'd our CI */
static int  ipcp_rejci __ARGS((fsm *, u_char *, int));	/* Peer rej'd our CI */
static int  ipcp_reqci __ARGS((fsm *, u_char *, int *, int)); /* Rcv CI */
static void ipcp_up __ARGS((fsm *));		/* We're UP */
static void ipcp_down __ARGS((fsm *));		/* We're DOWN */


fsm ipcp_fsm[_NPPP];		/* IPCP fsm structure */

static fsm_callbacks ipcp_callbacks = { /* IPCP callback routines */
    ipcp_resetci,		/* Reset our Configuration Information */
    ipcp_cilen,			/* Length of our Configuration Information */
    ipcp_addci,			/* Add our Configuration Information */
    ipcp_ackci,			/* ACK our Configuration Information */
    ipcp_nakci,			/* NAK our Configuration Information */
    ipcp_rejci,			/* Reject our Configuration Information */
    ipcp_reqci,			/* Request peer's Configuration Information */
    ipcp_up,			/* Called when fsm reaches OPENED state */
    ipcp_down,			/* Called when fsm leaves OPENED state */
    NULL,			/* Called when we want the lower layer up */
    NULL,			/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "IPCP"			/* String name of protocol */
};

/*
 * Lengths of configuration options.
 */
#define CILEN_VOID	2
#define CILEN_COMPRESS	4	/* min length for compression protocol opt. */
#define CILEN_VJ	6	/* length for RFC1332 Van-Jacobson opt. */
#define CILEN_ADDR	6	/* new-style single address option */
#define CILEN_ADDRS	10	/* old-style dual address option */


#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")


/*
 * Make a string representation of a network IP address.
 */
char *
ip_ntoa(ipaddr)
u_long ipaddr;
{
    static char b[64];

    ipaddr = ntohl(ipaddr);

    sprintf(b, "%d.%d.%d.%d",
	    (u_char)(ipaddr >> 24),
	    (u_char)(ipaddr >> 16),
	    (u_char)(ipaddr >> 8),
	    (u_char)(ipaddr));
    return b;
}


/*
 * ipcp_init - Initialize IPCP.
 */
void
ipcp_init(unit)
    int unit;
{
    fsm *f = &ipcp_fsm[unit];
    ipcp_options *wo = &ipcp_wantoptions[unit];
    ipcp_options *ao = &ipcp_allowoptions[unit];

    f->unit = unit;
    f->protocol = IPCP;
    f->callbacks = &ipcp_callbacks;
    fsm_init(&ipcp_fsm[unit]);

    wo->neg_addr = 1;
    wo->old_addrs = 0;
    wo->ouraddr = 0;
    wo->hisaddr = 0;

    wo->neg_vj = 1;
    wo->old_vj = 0;
    wo->vj_protocol = IPCP_VJ_COMP;
    wo->maxslotindex = MAX_STATES - 1; /* really max index */
    wo->cflag = 1;

    /* max slots and slot-id compression are currently hardwired in */
    /* ppp_if.c to 16 and 1, this needs to be changed (among other */
    /* things) gmc */

    ao->neg_addr = 1;
    ao->neg_vj = 1;
    ao->maxslotindex = MAX_STATES - 1;
    ao->cflag = 1;
}


/*
 * ipcp_open - IPCP is allowed to come up.
 */
void
ipcp_open(unit)
    int unit;
{
    fsm_open(&ipcp_fsm[unit]);
}


/*
 * ipcp_close - Take IPCP down.
 */
void
ipcp_close(unit)
    int unit;
{
    fsm_close(&ipcp_fsm[unit]);
}


/*
 * ipcp_lowerup - The lower layer is up.
 */
void
ipcp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ipcp_fsm[unit]);
}


/*
 * ipcp_lowerdown - The lower layer is down.
 */
void
ipcp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_input - Input IPCP packet.
 */
void
ipcp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm_input(&ipcp_fsm[unit], p, len);
}


/*
 * ipcp_protrej - A Protocol-Reject was received for IPCP.
 *
 * Pretend the lower layer went down, so we shut up.
 */
void
ipcp_protrej(unit)
    int unit;
{
    fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_resetci - Reset our CI.
 */
static void
ipcp_resetci(f)
    fsm *f;
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];

    wo->req_addr = wo->neg_addr && ipcp_allowoptions[f->unit].neg_addr;
    if (wo->ouraddr == 0)
	wo->accept_local = 1;
    if (wo->hisaddr == 0)
	wo->accept_remote = 1;
    ipcp_gotoptions[f->unit] = *wo;
    cis_received[f->unit] = 0;
}


/*
 * ipcp_cilen - Return length of our CI.
 */
static int
ipcp_cilen(f)
    fsm *f;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];

#define LENCIVJ(neg, old)	(neg ? (old? CILEN_COMPRESS : CILEN_VJ) : 0)
#define LENCIADDR(neg, old)	(neg ? (old? CILEN_ADDRS : CILEN_ADDR) : 0)

    return (LENCIADDR(go->neg_addr, go->old_addrs) +
	    LENCIVJ(go->neg_vj, go->old_vj));
}


/*
 * ipcp_addci - Add our desired CIs to a packet.
 */
static void
ipcp_addci(f, ucp, lenp)
    fsm *f;
    u_char *ucp;
    int *lenp;
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    int len = *lenp;

#define ADDCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if (len >= vjlen) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(vjlen, ucp); \
	    PUTSHORT(val, ucp); \
	    if (!old) { \
		PUTCHAR(maxslotindex, ucp); \
		PUTCHAR(cflag, ucp); \
	    } \
	    len -= vjlen; \
	} else \
	    neg = 0; \
    }

#define ADDCIADDR(opt, neg, old, val1, val2) \
    if (neg) { \
	int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
	if (len >= addrlen) { \
	    u_long l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(addrlen, ucp); \
	    l = ntohl(val1); \
	    PUTLONG(l, ucp); \
	    if (old) { \
		l = ntohl(val2); \
		PUTLONG(l, ucp); \
	    } \
	    len -= addrlen; \
	} else \
	    neg = 0; \
    }

    /*
     * First see if we want to change our options to the old
     * forms because we have received old forms from the peer.
     */
    if (wo->neg_addr && !go->neg_addr && !go->old_addrs) {
	/* use the old style of address negotiation */
	go->neg_addr = 1;
	go->old_addrs = 1;
    }
    if (wo->neg_vj && !go->neg_vj && !go->old_vj) {
	/* try an older style of VJ negotiation */
	if (cis_received[f->unit] == 0) {
	    /* keep trying the new style until we see some CI from the peer */
	    go->neg_vj = 1;
	} else {
	    /* use the old style only if the peer did */
	    if (ho->neg_vj && ho->old_vj) {
		go->neg_vj = 1;
		go->old_vj = 1;
		go->vj_protocol = ho->vj_protocol;
	    }
	}
    }

    ADDCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    ADDCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    *lenp -= len;
}


/*
 * ipcp_ackci - Ack our CIs.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
ipcp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_short cilen, citype, cishort;
    u_long cilong;
    u_char cimaxslotindex, cicflag;

    /*
     * CIs must be in exactly the same order that we sent...
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */

#define ACKCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if ((len -= vjlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != vjlen || \
	    citype != opt)  \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	    GETCHAR(cimaxslotindex, p); \
	    if (cimaxslotindex != maxslotindex) \
		goto bad; \
	    GETCHAR(cicflag, p); \
	    if (cicflag != cflag) \
		goto bad; \
	} \
    }

#define ACKCIADDR(opt, neg, old, val1, val2) \
    if (neg) { \
	int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
	u_long l; \
	if ((len -= addrlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != addrlen || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val1 != cilong) \
	    goto bad; \
	if (old) { \
	    GETLONG(l, p); \
	    cilong = htonl(l); \
	    if (val2 != cilong) \
		goto bad; \
	} \
    }

    ACKCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    ACKCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    IPCPDEBUG((LOG_INFO, "ipcp_ackci: received bad Ack!"));
    return (0);
}

/*
 * ipcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if IPCP is in the OPENED state.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static int
ipcp_nakci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, cicflag;
    u_char citype, cilen, *next;
    u_short cishort;
    u_long ciaddr1, ciaddr2, l;
    ipcp_options no;		/* options we've seen Naks for */
    ipcp_options try;		/* options to request next time */

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIADDR(opt, neg, old, code) \
    if (go->neg && \
	len >= (cilen = (old? CILEN_ADDRS: CILEN_ADDR)) && \
	p[1] == cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	if (old) { \
	    GETLONG(l, p); \
	    ciaddr2 = htonl(l); \
	    no.old_addrs = 1; \
	} else \
	    ciaddr2 = 0; \
	no.neg = 1; \
	code \
    }

#define NAKCIVJ(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_COMPRESS || cilen == CILEN_VJ) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	if (cilen == CILEN_VJ) { \
	    GETCHAR(cimaxslotindex, p); \
            GETCHAR(cicflag, p); \
        } \
	no.neg = 1; \
        code \
    }

    /*
     * Accept the peer's idea of {our,his} address, if different
     * from our idea, only if the accept_{local,remote} flag is set.
     */
    NAKCIADDR(CI_ADDR, neg_addr, go->old_addrs,
	      if (go->accept_local && ciaddr1) { /* Do we know our address? */
		  try.ouraddr = ciaddr1;
		  IPCPDEBUG((LOG_INFO, "local IP address %s",
			     ip_ntoa(ciaddr1)));
	      }
	      if (go->accept_remote && ciaddr2) { /* Does he know his? */
		  try.hisaddr = ciaddr2;
		  IPCPDEBUG((LOG_INFO, "remote IP address %s",
			     ip_ntoa(ciaddr2)));
	      }
	      );

    /*
     * Accept the peer's value of maxslotindex provided that it
     * is less than what we asked for.  Turn off slot-ID compression
     * if the peer wants.  Send old-style compress-type option if
     * the peer wants.
     */
    NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
	    if (cilen == CILEN_VJ) {
		if (cishort == IPCP_VJ_COMP) {
		    try.old_vj = 0;
		    if (cimaxslotindex < go->maxslotindex)
			try.maxslotindex = cimaxslotindex;
		    if (!cicflag)
			try.cflag = 0;
		} else {
		    try.neg_vj = 0;
		}
	    } else {
		if (cishort == IPCP_VJ_COMP || cishort == IPCP_VJ_COMP_OLD) {
		    try.old_vj = 1;
		    try.vj_protocol = cishort;
		} else {
		    try.neg_vj = 0;
		}
	    }
	    );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If they want to negotiate about IP addresses, we comply.
     * If they want us to ask for compression, we refuse.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if( (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_COMPRESSTYPE:
	    if (go->neg_vj || no.neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS))
		goto bad;
	    no.neg_vj = 1;
	    break;
	case CI_ADDRS:
	    if (go->neg_addr && go->old_addrs || no.old_addrs
		|| cilen != CILEN_ADDRS)
		goto bad;
	    try.neg_addr = 1;
	    try.old_addrs = 1;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    GETLONG(l, p);
	    ciaddr2 = htonl(l);
	    if (ciaddr2 && go->accept_remote)
		try.hisaddr = ciaddr2;
	    no.old_addrs = 1;
	    break;
	case CI_ADDR:
	    if (go->neg_addr || no.neg_addr || cilen != CILEN_ADDR)
		goto bad;
	    try.neg_addr = 1;
	    try.old_addrs = 0;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    no.neg_addr = 1;
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
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    IPCPDEBUG((LOG_INFO, "ipcp_nakci: received bad Nak!"));
    return 0;
}


/*
 * ipcp_rejci - Reject some of our CIs.
 */
static int
ipcp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, ciflag, cilen;
    u_short cishort;
    u_long cilong;
    ipcp_options try;		/* options to request next time */

    try = *go;
    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIADDR(opt, neg, old, val1, val2) \
    if (go->neg && \
	len >= (cilen = old? CILEN_ADDRS: CILEN_ADDR) && \
	p[1] == cilen && \
	p[0] == opt) { \
	u_long l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val1) \
	    goto bad; \
	if (old) { \
	    GETLONG(l, p); \
	    cilong = htonl(l); \
	    /* Check rejected value. */ \
	    if (cilong != val2) \
		goto bad; \
	} \
	try.neg = 0; \
    }

#define REJCIVJ(opt, neg, val, old, maxslot, cflag) \
    if (go->neg && \
	p[1] == (old? CILEN_COMPRESS : CILEN_VJ) && \
	len >= p[1] && \
	p[0] == opt) { \
	len -= p[1]; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */  \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	   GETCHAR(cimaxslotindex, p); \
	   if (cimaxslotindex != maxslot) \
	     goto bad; \
	   GETCHAR(ciflag, p); \
	   if (ciflag != cflag) \
	     goto bad; \
        } \
	try.neg = 0; \
     }

    REJCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    REJCIVJ(CI_COMPRESSTYPE, neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

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
    IPCPDEBUG((LOG_INFO, "ipcp_rejci: received bad Reject!"));
    return 0;
}


/*
 * ipcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
ipcp_reqci(f, inp, len, reject_if_disagree)
    fsm *f;
    u_char *inp;		/* Requested CIs */
    int *len;			/* Length of requested CIs */
    int reject_if_disagree;
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    ipcp_options *ao = &ipcp_allowoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_long tl, ciaddr1, ciaddr2;/* Parsed address values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */
    u_char maxslotindex, cflag;

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
	    IPCPDEBUG((LOG_INFO, "ipcp_reqci: bad CI length!"));
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
	case CI_ADDRS:
	    IPCPDEBUG((LOG_INFO, "ipcp: received ADDRS "));
	    if (!ao->neg_addr ||
		cilen != CILEN_ADDRS) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);		/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    IPCPDEBUG((LOG_INFO, "(%s:", ip_ntoa(ciaddr1)));
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof (long), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    }

	    /*
	     * If he doesn't know our address, or if we both have our address
	     * but disagree about it, then NAK it with our idea.
	     */
	    GETLONG(tl, p);		/* Parse desination address (ours) */
	    ciaddr2 = htonl(tl);
	    IPCPDEBUG((LOG_INFO, "%s)", ip_ntoa(ciaddr2)));
	    if (ciaddr2 != wo->ouraddr) {
		if (ciaddr2 == 0 || !wo->accept_local) {
		    orc = CONFNAK;
		    if (!reject_if_disagree) {
			DECPTR(sizeof (long), p);
			tl = ntohl(wo->ouraddr);
			PUTLONG(tl, p);
		    }
		} else {
		    go->ouraddr = ciaddr2;	/* accept peer's idea */
		}
	    }

	    ho->neg_addr = 1;
	    ho->old_addrs = 1;
	    ho->hisaddr = ciaddr1;
	    ho->ouraddr = ciaddr2;
	    break;

	case CI_ADDR:
	    IPCPDEBUG((LOG_INFO, "ipcp: received ADDR "));

	    if (!ao->neg_addr ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);	/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    IPCPDEBUG((LOG_INFO, "(%s)", ip_ntoa(ciaddr1)));
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof (long), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    }
	
	    ho->neg_addr = 1;
	    ho->hisaddr = ciaddr1;
	    break;
	
	case CI_COMPRESSTYPE:
	    IPCPDEBUG((LOG_INFO, "ipcp: received COMPRESSTYPE "));
	    if (!ao->neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS)) {
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);
	    IPCPDEBUG((LOG_INFO, "(%d)", cishort));

	    if (!(cishort == IPCP_VJ_COMP ||
		  (cishort == IPCP_VJ_COMP_OLD && cilen == CILEN_COMPRESS))) {
		orc = CONFREJ;
		break;
	    }

	    ho->neg_vj = 1;
	    ho->vj_protocol = cishort;
	    if (cilen == CILEN_VJ) {
		GETCHAR(maxslotindex, p);
		if (maxslotindex > ao->maxslotindex) { 
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(ao->maxslotindex, p);
		    }
		}
		GETCHAR(cflag, p);
		if (cflag && !ao->cflag) {
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(wo->cflag, p);
		    }
		}
		ho->maxslotindex = maxslotindex;
		ho->cflag = wo->cflag;
	    }
	    break;

	default:
	    orc = CONFREJ;
	    break;
	}

endswitch:
	IPCPDEBUG((LOG_INFO, " (%s)\n", CODENAME(orc)));

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

	/* Need to move CI? */
	if (ucp != cip)
	    BCOPY(cip, ucp, cilen);	/* Move it */

	/* Update output pointer */
	INCPTR(cilen, ucp);
    }

    /*
     * If we aren't rejecting this packet, and we want to negotiate
     * their address, and they didn't send their address, then we
     * send a NAK with a CI_ADDR option appended.  We assume the
     * input buffer is long enough that we can append the extra
     * option safely.
     */
    if (rc != CONFREJ && !ho->neg_addr &&
	wo->req_addr && !reject_if_disagree) {
	if (rc == CONFACK) {
	    rc = CONFNAK;
	    ucp = inp;			/* reset pointer */
	    wo->req_addr = 0;		/* don't ask again */
	}
	PUTCHAR(CI_ADDR, ucp);
	PUTCHAR(CILEN_ADDR, ucp);
	tl = ntohl(wo->hisaddr);
	PUTLONG(tl, ucp);
    }

    *len = ucp - inp;			/* Compute output length */
    IPCPDEBUG((LOG_INFO, "ipcp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}


/*
 * ipcp_up - IPCP has come UP.
 *
 * Configure the IP network interface appropriately and bring it up.
 */
static void
ipcp_up(f)
    fsm *f;
{
    u_long mask;
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];

    IPCPDEBUG((LOG_INFO, "ipcp: up"));
    go->default_route = 0;
    go->proxy_arp = 0;

    /*
     * We must have a non-zero IP address for both ends of the link.
     */
    if (!ho->neg_addr)
	ho->hisaddr = ipcp_wantoptions[f->unit].hisaddr;

    if (ho->hisaddr == 0) {
	syslog(LOG_ERR, "Could not determine remote IP address");
	ipcp_close(f->unit);
	return;
    }
    if (go->ouraddr == 0) {
	syslog(LOG_ERR, "Could not determine local IP address");
	ipcp_close(f->unit);
	return;
    }

    /*
     * Check that the peer is allowed to use the IP address he wants.
     */
    if (!auth_ip_addr(f->unit, ho->hisaddr)) {
	syslog(LOG_ERR, "Peer is not authorized to use remote address %s",
	       ip_ntoa(ho->hisaddr));
	ipcp_close(f->unit);
	return;
    }

    syslog(LOG_NOTICE, "local  IP address %s", ip_ntoa(go->ouraddr));
    syslog(LOG_NOTICE, "remote IP address %s", ip_ntoa(ho->hisaddr));

    /*
     * Set IP addresses and (if specified) netmask.
     */
    mask = GetMask(go->ouraddr);
    if (!sifaddr(f->unit, go->ouraddr, ho->hisaddr, mask)) {
	IPCPDEBUG((LOG_WARNING, "sifaddr failed"));
	ipcp_close(f->unit);
	return;
    }

    /* set tcp compression */
    sifvjcomp(f->unit, ho->neg_vj, ho->cflag);

    /* bring the interface up for IP */
    if (!sifup(f->unit)) {
	IPCPDEBUG((LOG_WARNING, "sifup failed"));
	ipcp_close(f->unit);
	return;
    }

    /* assign a default route through the interface if required */
    if (ipcp_wantoptions[f->unit].default_route) 
	if (sifdefaultroute(f->unit, ho->hisaddr))
	    go->default_route = 1;

    /* Make a proxy ARP entry if requested. */
    if (ipcp_wantoptions[f->unit].proxy_arp)
	if (sifproxyarp(f->unit, ho->hisaddr))
	    go->proxy_arp = 1;
}


/*
 * ipcp_down - IPCP has gone DOWN.
 *
 * Take the IP network interface down, clear its addresses
 * and delete routes through it.
 */
static void
ipcp_down(f)
    fsm *f;
{
    u_long ouraddr, hisaddr;

    IPCPDEBUG((LOG_INFO, "ipcp: down"));

    ouraddr = ipcp_gotoptions[f->unit].ouraddr;
    hisaddr = ipcp_hisoptions[f->unit].hisaddr;
    if (ipcp_gotoptions[f->unit].proxy_arp)
	cifproxyarp(f->unit, hisaddr);
    if (ipcp_gotoptions[f->unit].default_route) 
	cifdefaultroute(f->unit, hisaddr);
    sifdown(f->unit);
    cifaddr(f->unit, ouraddr, hisaddr);
}
