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

/*
 * TODO:
 * Fix IP address negotiation (wantoptions or hisoptions).
 * Don't set zero IP addresses.
 * Send NAKs for unsent CIs.
 * VJ compression.
 */

#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <string.h>

#ifndef BSD
#ifndef sun
#define BSD 44
#endif
#endif /*BSD*/

#ifdef STREAMS
#include <sys/stream.h>
#include "ppp_str.h"
#endif
  
#include "pppd.h"
#include <net/if_ppp.h>

#include <net/ppp.h>
#include "fsm.h"
#include "ipcp.h"


/* global vars */
ipcp_options ipcp_wantoptions[NPPP]; /* Options that we want to request */
ipcp_options ipcp_gotoptions[NPPP]; /* Options that peer ack'd */
ipcp_options ipcp_allowoptions[NPPP]; /* Options that we allow peer to
					 request */
ipcp_options ipcp_hisoptions[NPPP]; /* Options that we ack'd */

/* local vars */

/*
 * VJ compression protocol mode for negotiation. See ipcp.h for a 
 * description of each mode.
 */
static int vj_mode = IPCP_VJMODE_RFC1332;

static int vj_opt_len = 6;	/* holds length in octets for valid vj */
				/* compression frame depending on mode */

static int vj_opt_val = IPCP_VJ_COMP;
				/* compression negotiation frames */
				/* depending on vj_mode */

static void ipcp_resetci __ARGS((fsm *));	/* Reset our Configuration Information */
static int ipcp_cilen __ARGS((fsm *));	        /* Return length of our CI */
static void ipcp_addci __ARGS((fsm *, u_char *));	/* Add our CIs */
static int ipcp_ackci __ARGS((fsm *, u_char *, int));	/* Ack some CIs */
static void ipcp_nakci __ARGS((fsm *, u_char *, int));	/* Nak some CIs */
static void ipcp_rejci __ARGS((fsm *, u_char *, int));	/* Reject some CIs */
static u_char ipcp_reqci __ARGS((fsm *, u_char *, int *));	/* Check the requested CIs */
static void ipcp_up __ARGS((fsm *));		/* We're UP */
static void ipcp_down __ARGS((fsm *));	/* We're DOWN */


static fsm ipcp_fsm[NPPP];	/* IPCP fsm structure */

static fsm_callbacks ipcp_callbacks = { /* IPCP callback routines */
    ipcp_resetci,		/* Reset our Configuration Information */
    ipcp_cilen,			/* Length of our Configuration Information */
    ipcp_addci,			/* Add our Configuration Information */
    ipcp_ackci,			/* ACK our Configuration Information */
    ipcp_nakci,			/* NAK our Configuration Information */
    ipcp_rejci,			/* Reject our Configuration Information */
    ipcp_reqci,			/* Request peer's Configuration Information */
    ipcp_up,			/* Called when fsm reaches OPEN state */
    ipcp_down,			/* Called when fsm leaves OPEN state */
    NULL,			/* Called when fsm reaches CLOSED state */
    NULL,			/* Called when Protocol-Reject received */
    NULL			/* Retransmission is necessary */
};

char *
ip_ntoa(ipaddr)
u_long ipaddr;
{
    static char b1[64], b2[64], w = 0;
    char *b = (w++&1) ? b1 : b2;

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
    f->timeouttime = DEFTIMEOUT;
    f->maxconfreqtransmits = DEFMAXCONFIGREQS;
    f->maxtermtransmits = DEFMAXTERMTRANSMITS;
    f->maxnakloops = DEFMAXNAKLOOPS;
    f->callbacks = &ipcp_callbacks;

    wo->neg_addrs = 1;
    wo->ouraddr = 0;
    wo->hisaddr = 0;

    wo->neg_vj = 1;
    wo->maxslotindex = MAX_STATES - 1; /* really max index */
    wo->cflag = 1;

    /* max slots and slot-id compression are currently hardwired in */
    /* ppp_if.c to 16 and 1, this needs to be changed (among other */
    /* things) gmc */

    ao->neg_addrs = 1;		/* accept old style dual addr */
    ao->neg_addr = 1;		/* accept new style single addr */
    ao->neg_vj = 1;
    ao->maxslotindex = MAX_STATES - 1;
    ao->cflag = 1;
    fsm_init(&ipcp_fsm[unit]);
}

/*
 * ipcp_vj_setmode - set option length and option value for vj
 * compression negotiation frames depending on mode
 */

void
  ipcp_vj_setmode(mode)
int mode;    
{
  vj_mode = mode;

  switch (vj_mode) {

  case IPCP_VJMODE_OLD:		/* with wrong code (0x0037) */
    vj_opt_len = 4;
    vj_opt_val = IPCP_VJ_COMP_OLD;
    break;

  case IPCP_VJMODE_RFC1172:	/* as per rfc1172 */
    vj_opt_len = 4;
    vj_opt_val = IPCP_VJ_COMP;
    break;

  case IPCP_VJMODE_RFC1332:     /* draft mode vj compression */          
    vj_opt_len = 6;	      /* negotiation includes values for */    
                              /* maxslot and slot number compression */
    vj_opt_val = IPCP_VJ_COMP;
    break;

  default:
    IPCPDEBUG((LOG_WARNING, "Unknown vj compression mode %d.  Please report \
this error.", vj_mode))
    break;
  }

}
/*
 * ipcp_activeopen - Actively open IPCP.
 */
void
  ipcp_activeopen(unit)
int unit;
{
    fsm_activeopen(&ipcp_fsm[unit]);
}


/*
 * ipcp_passiveopen - Passively open IPCP.
 */
void ipcp_passiveopen(unit)
    int unit;
{
    fsm_passiveopen(&ipcp_fsm[unit]);
}


/*
 * ipcp_close - Close IPCP.
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
 * Simply pretend that LCP went down.
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
    ipcp_gotoptions[f->unit] = ipcp_wantoptions[f->unit];
}


/*
 * ipcp_cilen - Return length of our CI.
 */
static int
  ipcp_cilen(f)
fsm *f;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];


#define LENCISHORT(neg)  (neg ? vj_opt_len : 0)

#define LENCIADDRS(neg)  (neg ? 10 : 0)

#define LENCIADDR(neg)  (neg ? 6 : 0)

    return (LENCIADDRS(go->neg_addrs) +
    		LENCIADDR(go->neg_addr) +
			LENCISHORT(go->neg_vj));
}


/*
 * ipcp_addci - Add our desired CIs to a packet.
 */
static void
  ipcp_addci(f, ucp)
fsm *f;
u_char *ucp;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];


#define ADDCISHORT(opt, neg, val, maxslotindex, cflag) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(vj_opt_len, ucp); \
	PUTSHORT(val, ucp); \
	if (vj_mode == IPCP_VJMODE_RFC1332) { \
	   PUTCHAR(maxslotindex, ucp); \
	   PUTCHAR(cflag, ucp); \
	} \
    }

#define ADDCIADDRS(opt, neg, val1, val2) \
    if (neg) { \
	u_long l; \
	PUTCHAR(opt, ucp); \
	PUTCHAR(2 + 2 * sizeof (long), ucp); \
	l = ntohl(val1); \
	PUTLONG(l, ucp); \
	l = ntohl(val2); \
	PUTLONG(l, ucp); \
    }

#define ADDCIADDR(opt, neg, val) \
    if (neg) { \
	u_long l; \
	PUTCHAR(opt, ucp); \
	PUTCHAR(2 + sizeof (long), ucp); \
	l = ntohl(val); \
	PUTLONG(l, ucp); \
    }

    ADDCIADDRS(CI_ADDRS, go->neg_addrs, go->ouraddr, go->hisaddr)

    ADDCIADDR(CI_ADDR, go->neg_addr, go->ouraddr)

    ADDCISHORT(CI_COMPRESSTYPE, go->neg_vj, vj_opt_val,
	       go->maxslotindex, go->cflag)
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
#define ACKCISHORT(opt, neg, val, maxslotindex, cflag) \
    if (neg) { \
	if ((len -= vj_opt_len) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != vj_opt_len || \
	    citype != opt)  \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	if (vj_mode == IPCP_VJMODE_RFC1332) { \
	  GETCHAR(cimaxslotindex, p); \
	  if (cimaxslotindex > maxslotindex) \
	    goto bad; \
	  GETCHAR(cicflag, p); \
	  if (cicflag != cflag) \
	    goto bad; \
	} \
    }

#define ACKCIADDRS(opt, neg, val1, val2) \
    if (neg) { \
	u_long l; \
	if ((len -= 2 + 2 * sizeof (long)) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 2 + 2 * sizeof (long) || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val1) { \
	    if (val1 != cilong) \
		goto bad; \
	} \
	else \
	    val1 = cilong; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val2) { \
	    if (val2 != cilong) \
		goto bad; \
	} \
	else \
	    val2 = cilong; \
    }

#define ACKCIADDR(opt, neg, val) \
    if (neg) { \
	u_long l; \
	if ((len -= 2 + sizeof (long)) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != 2 + sizeof (long) || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val) { \
	    if (val != cilong) \
		goto bad; \
	} \
	else \
	    val = cilong; \
    }

    ACKCIADDRS(CI_ADDRS, go->neg_addrs, go->ouraddr, go->hisaddr)
    ACKCIADDR(CI_ADDR, go->neg_addr, go->ouraddr)
    ACKCISHORT(CI_COMPRESSTYPE, go->neg_vj, vj_opt_val, go->maxslotindex, go->cflag)
    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    IPCPDEBUG((LOG_INFO, "ipcp_ackci: received bad Ack!"));

    if (vj_mode == IPCP_VJMODE_RFC1332 )
      IPCPDEBUG((LOG_INFO, "ipcp_ackci: citype %d, cilen %l",
		 citype, cilen));

    if (citype == CI_COMPRESSTYPE)  {
      IPCPDEBUG((LOG_INFO, "ipcp_ackci: compress_type %d", cishort));
      if (vj_mode == IPCP_VJMODE_RFC1332)
	IPCPDEBUG((LOG_INFO, ", maxslotindex %d, cflag %d",
		   cishort, cimaxslotindex, cicflag));
    }
    return (0);
}

/*
 * ipcp_nakci - NAK some of our CIs.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static void
  ipcp_nakci(f, p, len)
fsm *f;
u_char *p;
int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, cicflag;
    u_short cishort;
    u_long ciaddr1, ciaddr2;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCISHORT(opt, neg, code) \
    if (neg && \
	len >= vj_opt_len && \
	p[1] == vj_opt_len && \
	p[0] == opt) { \
	  len -= vj_opt_len; \
	  INCPTR(2, p); \
	  GETSHORT(cishort, p); \
	  if (vj_mode == IPCP_VJMODE_RFC1332) { \
	     GETCHAR(cimaxslotindex, p); \
             GETCHAR(cicflag, p); \
          } \
          code \
    }

#define NAKCIADDRS(opt, neg, code) \
    if (neg && \
	len >= 2 + 2 * sizeof (long) && \
	p[1] == 2 + 2 * sizeof (long) && \
	p[0] == opt) { \
	u_long l; \
	len -= 2 + 2 * sizeof (long); \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	GETLONG(l, p); \
	ciaddr2 = htonl(l); \
	code \
    }

#define NAKCIADDR(opt, neg, code) \
    if (neg && \
	len >= 2 + sizeof (long) && \
	p[1] == 2 + sizeof (long) && \
	p[0] == opt) { \
	u_long l; \
	len -= 2 + sizeof (long); \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	code \
    }

    NAKCIADDRS(CI_ADDRS, go->neg_addrs,
	       if (!go->ouraddr) {	/* Didn't know our address? */
		   syslog(LOG_INFO, "local IP address %s", ip_ntoa(ciaddr1));
		   go->ouraddr = ciaddr1;
	       }
	       if (ciaddr2) {		/* Does he know his? */
	           go->hisaddr = ciaddr2;
		   syslog(LOG_INFO, "remote IP address %s", ip_ntoa(ciaddr2));
	       }
	       )

    NAKCIADDR(CI_ADDR, go->neg_addr,
	       logf(LOG_INFO, "acquired IP address %s", ip_ntoa(ciaddr1));
	       if (!go->ouraddr) {	/* Didn't know our address? */
	      	   go->ouraddr = ciaddr1;
		   syslog(LOG_INFO, "remote IP address %s", ip_ntoa(ciaddr1));
	       }
	       )
	       
    NAKCISHORT(CI_COMPRESSTYPE, go->neg_vj,
       	       if (cishort != vj_opt_val)
	          goto bad;
	       go->maxslotindex = cimaxslotindex; /* this is what it */
	       go->cflag = cicflag;		  /* wants  */

	       )
    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len == 0)
	return;
bad:
    IPCPDEBUG((LOG_INFO, "ipcp_nakci: received bad Nak!"));
}


/*
 * ipcp_rejci - Reject some of our CIs.
 */
static void
  ipcp_rejci(f, p, len)
fsm *f;
u_char *p;
int len;
{
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char cimaxslotindex, ciflag;
    u_short cishort;
    u_long cilong;

    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCISHORT(opt, neg, val, maxslot, cflag) \
    if (neg && \
	len >= vj_opt_len && \
	p[1] == vj_opt_len && \
	p[0] == opt) { \
	len -= vj_opt_len; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */  \
	if (cishort != val) \
	    goto bad; \
	if (vj_mode == IPCP_VJMODE_RFC1332) { \
	   GETCHAR(cimaxslotindex, p); \
	   if (cimaxslotindex != maxslot) \
	     goto bad; \
	   GETCHAR(ciflag, p); \
	   if (ciflag != cflag) \
	     goto bad; \
        } \
	neg = 0; \
     }

#define REJCIADDRS(opt, neg, val1, val2) \
    if (neg && \
	len >= 2 + 2 * sizeof (long) && \
	p[1] == 2 + 2 * sizeof (long) && \
	p[0] == opt) { \
	u_long l; \
	len -= 2 + 2 * sizeof (long); \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val2) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val1) \
	    goto bad; \
	neg = 0; \
    }

#define REJCIADDR(opt, neg, val) \
    if (neg && \
	len >= 2 + sizeof (long) && \
	p[1] == 2 + sizeof (long) && \
	p[0] == opt) { \
	u_long l; \
	len -= 2 + sizeof (long); \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val) \
	    goto bad; \
	neg = 0; \
    }

    REJCIADDRS(CI_ADDRS, go->neg_addrs, go->ouraddr, go->hisaddr)

    REJCIADDR(CI_ADDR, go->neg_addr, go->ouraddr)

    REJCISHORT(CI_COMPRESSTYPE, go->neg_vj, vj_opt_val, go->maxslotindex, go->cflag)

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len == 0)
	return;

bad:
    IPCPDEBUG((LOG_INFO, "ipcp_rejci: received bad Reject!"));
}


/*
 * ipcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.
 */
static u_char
  ipcp_reqci(f, inp, len)
fsm *f;
u_char *inp;		/* Requested CIs */
int *len;			/* Length of requested CIs */
{
    ipcp_options *wo = &ipcp_wantoptions[f->unit];
    ipcp_options *ho = &ipcp_hisoptions[f->unit];
    ipcp_options *ao = &ipcp_allowoptions[f->unit];
    ipcp_options *go = &ipcp_gotoptions[f->unit];
    u_char *cip;		/* Pointer to Current CI */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_long tl, ciaddr1, ciaddr2;	/* Parsed address values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p = inp;		/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */
    u_char maxslotindex, cflag;

    /*
     * Reset all his options.
     */
    ho->neg_addrs = 0;
    ho->neg_vj = 0;
    ho->maxslotindex = 0;
    ho->cflag = 0;
    
    /*
     * Process all his options.
     */
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p;			/* Remember begining of CI */
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
	cilen -= 2;			/* Adjust cilen to just data */

	switch (citype) {		/* Check CI type */
	  case CI_ADDRS:
	    logf(LOG_INFO, "ipcp: received ADDRS ");
	    if (!ao->neg_addrs ||
		cilen != 2 * sizeof (long))
	    {	/* Check CI length */
		INCPTR(cilen, p); 	/* Skip rest of CI */
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
	    if (!ciaddr1 ||
		(wo->neg_addrs && wo->hisaddr && ciaddr1 != wo->hisaddr))
	    {
		orc = CONFNAK;
		DECPTR(sizeof (long), p);
		tl = wo->neg_addrs ? ntohl(wo->hisaddr) : 0;
		PUTLONG(tl, p);
	    }

	    /*
	     * If he doesn't know our address, or if we both have our address
	     * but disagree about it, then NAK it with our idea.
	     */
	    GETLONG(tl, p);		/* Parse desination address (ours) */
	    ciaddr2 = htonl(tl);
	    logf(LOG_INFO, "(%s:%s)", ip_ntoa(ciaddr1), ip_ntoa(ciaddr2));
	    if (!ciaddr2 ||
		(wo->neg_addrs && wo->ouraddr && ciaddr2 != wo->ouraddr))
	    {
		orc = CONFNAK;
		DECPTR(sizeof (long), p);
		tl = ntohl(wo->ouraddr);
		PUTLONG(tl, p);
	    }
	    if (orc == CONFNAK)
		break;

	    /* XXX ho or go? */
	    ho->neg_addrs = 1;
	    ho->hisaddr = ciaddr1;
	    ho->ouraddr = ciaddr2;
	    break;

	  case CI_ADDR:
	    logf(LOG_INFO, "ipcp: received ADDR ");
	    go->got_addr = 1;
	    go->neg_addrs = 0;
	    go->neg_addr = 1;

	    if (!ao->neg_addr ||
		cilen != sizeof (long)) { /* Check CI length */
		INCPTR(cilen, p); /* Skip rest of CI */
		orc = CONFREJ;	/* Reject CI */
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
	    logf(LOG_INFO, "(%s)", ip_ntoa(ciaddr1));
	    if (!ciaddr1 ||
		(wo->neg_addr && wo->hisaddr && ciaddr1 != wo->hisaddr)) {
		orc = CONFNAK;
		DECPTR(sizeof (long), p);
		tl = wo->neg_addr ? ntohl(wo->hisaddr) : 0;
		PUTLONG(tl, p);
	    }
	
	    if (orc == CONFNAK)
		break;
	
	    /* XXX ho or go? */
	    ho->neg_addr = 1;
	    ho->hisaddr = ciaddr1;
	    break;
	
	  case CI_COMPRESSTYPE:
	    logf(LOG_INFO, "ipcp: received COMPRESSTYPE ");
	    if (!ao->neg_vj ||
		cilen != (vj_opt_len  - 2)) {
		INCPTR(cilen, p);
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);
	    logf(LOG_INFO, "(%d)", cishort);

	    /*
	     * Compresstype must be vj_opt_val.
	     */
	    if (cishort != vj_opt_val) {
		DECPTR(sizeof (short), p);
		orc = CONFNAK;
		PUTSHORT(vj_opt_val, p);
		break;
	    }
	    ho->neg_vj = 1;
	    if (vj_mode == IPCP_VJMODE_RFC1332) {
	      GETCHAR(maxslotindex, p);
	      if (maxslotindex > wo->maxslotindex) { 
		DECPTR(1, p);
		orc = CONFNAK;
		PUTCHAR(wo->maxslotindex, p);
		break;
	      }
	      ho->maxslotindex = maxslotindex;

	      GETCHAR(cflag, p);
	      if (cflag != wo->cflag) {
		DECPTR(1, p);
		orc = CONFNAK;
		PUTCHAR(wo->cflag, p);
		break;
	      }
	      ho->cflag = wo->cflag;
	    }
	    break;

	  default:
	    INCPTR(cilen, p);
	    orc = CONFREJ;
	    break;
	}
	cilen += 2;			/* Adjust cilen whole CI */

endswitch:
	logf(LOG_INFO, " (%s)\n",
	     orc == CONFACK ? "ACK" : (orc == CONFNAK ? "NAK" : "Reject"));

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

	/* Need to move CI? */
	if (ucp != cip)
	    /* Move it */
	    memcpy(ucp, cip, (size_t)cilen);

	/* Update output pointer */
	INCPTR(cilen, ucp);
    }

    /*
     * XXX If we wanted to send additional NAKs (for unsent CIs), the
     * code would go here.  This must be done with care since it might
     * require a longer packet than we received.
     */

    *len = ucp - inp;			/* Compute output length */

    syslog(LOG_INFO, "ipcp: returning Configure-%s",
	   rc == CONFACK ? "ACK" :
	   rc == CONFNAK ? "NAK" : "Reject");

    return (rc);			/* Return final code */
}


/*
 * ipcp_up - IPCP has come UP.
 */
static void
  ipcp_up(f)
fsm *f;
{
  u_long mask;

  syslog(LOG_INFO, "ipcp: up");

  if (ipcp_hisoptions[f->unit].hisaddr == 0)
      ipcp_hisoptions[f->unit].hisaddr = ipcp_wantoptions[f->unit].hisaddr;

  syslog(LOG_INFO, "local  IP address %s",
	 ip_ntoa(ipcp_gotoptions[f->unit].ouraddr));
  syslog(LOG_INFO, "remote IP address %s",
	 ip_ntoa(ipcp_hisoptions[f->unit].hisaddr));

  SIFADDR(f->unit, ipcp_gotoptions[f->unit].ouraddr,
	  ipcp_hisoptions[f->unit].hisaddr);

  /* set new netmask if specified */
  mask = GetMask(ipcp_gotoptions[f->unit].ouraddr);
  if (mask) 
    SIFMASK(f->unit, mask);

  /* set tcp compression */
  SIFVJCOMP(f->unit, ipcp_hisoptions[f->unit].neg_vj);
}


/*
 * ipcp_down - IPCP has gone DOWN.
 *
 * Alert other protocols.
 */
static void
  ipcp_down(f)
fsm *f;
{
  syslog(LOG_INFO, "ipcp: down");

  CIFADDR(f->unit, ipcp_gotoptions[f->unit].ouraddr,
	  ipcp_hisoptions[f->unit].hisaddr);
}
