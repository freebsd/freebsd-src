/*
 * fsm.c - {Link, IP} Control Protocol Finite State Machine.
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
 * Mechanism to exit() and/or drop DTR.
 * Hold-down on open?
 * Randomize fsm id on link/init.
 * Deal with variable outgoing MTU.
 */

#include <stdio.h>
#include <sys/types.h>
/*#include <malloc.h>*/
#include <syslog.h>

#ifdef STREAMS
#include	<sys/stream.h>
#include	<sys/socket.h>
#include	<net/if.h>
#endif

#include <net/ppp.h>
#include "pppd.h"
#include "fsm.h"

extern char *proto_name();

static void fsm_timeout __ARGS((caddr_t));
static void fsm_rconfack __ARGS((fsm *, u_char *, int, int));
static void fsm_rconfnak __ARGS((fsm *, u_char *, int, int));
static void fsm_rconfrej __ARGS((fsm *, u_char *, int, int));
static void fsm_rtermreq __ARGS((fsm *, int));
static void fsm_rtermack __ARGS((fsm *));
static void fsm_rcoderej __ARGS((fsm *, u_char *, int));
static void fsm_rprotrej __ARGS((fsm *, u_char *, int));
static void fsm_sconfreq __ARGS((fsm *));


/*
 * fsm_init - Initialize fsm.
 *
 * Initialize fsm state.
 */
void
  fsm_init(f)
fsm *f;
{
    f->state = CLOSED;
    f->flags = 0;
    f->id = 0;				/* XXX Start with random id? */
}


/*
 * fsm_activeopen - Actively open connection.
 *
 * Set new state, reset desired options and send requests.
 */
void
  fsm_activeopen(f)
fsm *f;
{
    f->flags &= ~(AOPENDING|POPENDING); /* Clear pending flags */
    if (f->state == REQSENT ||		/* Already actively open(ing)? */
	f->state == ACKRCVD ||
	f->state == ACKSENT ||
	f->state == OPEN)
	return;
    if (f->state == TERMSENT ||		/* Closing or */
	!(f->flags & LOWERUP)) {	/*  lower layer down? */
	f->flags |= AOPENDING;		/* Wait for desired event */
	return;
    }
    if (f->callbacks->resetci)
	(*f->callbacks->resetci)(f);	/* Reset options */
    fsm_sconfreq(f);			/* Send Configure-Request */
    TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
    f->state = REQSENT;
    f->retransmits = 0;			/* Reset retransmits count */
    f->nakloops = 0;			/* Reset nakloops count */
}


/*
 * fsm_passiveopen - Passively open connection.
 *
 * Set new state and reset desired options.
 */
void
  fsm_passiveopen(f)
fsm *f;
{
    f->flags &= ~(AOPENDING|POPENDING); /* Clear pending flags */
    if (f->state == LISTEN ||		/* Already passively open(ing)? */
	f->state == OPEN)
	return;
    if (f->state == REQSENT ||		/* Active-Opening or */
	f->state == ACKRCVD ||
	f->state == ACKSENT ||
	f->state == TERMSENT ||		/*  closing or */
	!(f->flags & LOWERUP)) {	/*  lower layer down? */
	f->flags |= POPENDING;		/* Wait for desired event */
	return;
    }
    if (f->callbacks->resetci)
	(*f->callbacks->resetci)(f);	/* Reset options */
    f->state = LISTEN;
    f->retransmits = 0;			/* Reset retransmits count */
    f->nakloops = 0;			/* Reset nakloops count */
}


/*
 * fsm_close - Start closing connection.
 *
 * Cancel timeouts and either initiate close or possibly go directly to
 * the CLOSED state.
 */
void
  fsm_close(f)
fsm *f;
{
    f->flags &= ~(AOPENDING|POPENDING); /* Clear pending flags */
    if (f->state == CLOSED ||		/* Already CLOSED or Closing? */
	f->state == TERMSENT)
	return;
    if (f->state == REQSENT ||		/* Timeout pending for Open? */
	f->state == ACKRCVD ||
	f->state == ACKSENT)
	UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
    if (f->state == OPEN &&		/* Open? */
	f->callbacks->down)
	(*f->callbacks->down)(f);	/* Inform upper layers we're down */
    if (f->state == ACKSENT ||		/* Could peer be OPEN? */
	f->state == OPEN) {
	fsm_sdata(f, TERMREQ, f->reqid = ++f->id, NULL, 0);
					/* Send Terminate-Request */
	TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
	f->state = TERMSENT;
	f->retransmits = 0;		/* Reset retransmits count */
    }
    else {
	f->state = CLOSED;
	if (f->callbacks->closed)
	    (*f->callbacks->closed)(f);	/* Exit/restart/etc. */
    }
}


/*
 * fsm_timeout - Timeout expired.
 */
static void
  fsm_timeout(arg)
caddr_t arg;
{
  fsm *f = (fsm *) arg;
    switch (f->state) {
      case REQSENT:
      case ACKRCVD:
      case ACKSENT:
	if (f->flags & POPENDING) {	/* Go passive? */
	    f->state = CLOSED;		/* Pretend for a moment... */
	    fsm_passiveopen(f);
	    return;
	}
	if (f->retransmits > f->maxconfreqtransmits) {
	    if (f->nakloops > f->maxnakloops) {
		syslog(LOG_INFO, "%s: timeout sending Config-Requests",
		       proto_name(f->protocol));
	    } else
		syslog(LOG_INFO, "%s: timed out. Config-Requests not accepted",
		       proto_name(f->protocol));

	    /* timeout sending config-requests */
	    fsm_close(f);

	    return;
	}
	if (f->callbacks->retransmit)	/* If there is a retransmit rtn? */
	    (*f->callbacks->retransmit)(f);
	fsm_sconfreq(f);		/* Send Configure-Request */
	TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
	f->state = REQSENT;
	++f->retransmits;
	f->nakloops = 0;
	break;

      case TERMSENT:
	if (f->flags & POPENDING) {	/* Go passive? */
	    f->state = CLOSED;		/* Pretend for a moment... */
	    fsm_passiveopen(f);
	    return;
	}
	if (++f->retransmits > f->maxtermtransmits) {
	    /*
	     * We've waited for an ack long enough.  Peer probably heard us.
	     */
	    f->state = CLOSED;
	    if (f->callbacks->closed)
		(*f->callbacks->closed)(f); /* Exit/restart/etc. */
	    return;
	}
	if (f->callbacks->retransmit)	/* If there is a retransmit rtn? */
	    (*f->callbacks->retransmit)(f);
	fsm_sdata(f, TERMREQ, f->reqid = ++f->id, NULL, 0);
					/* Send Terminate-Request */
	TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
	++f->retransmits;
    }
}


/*
 * fsm_lowerup - The lower layer is up.
 *
 * Start Active or Passive Open if pending.
 */
void
  fsm_lowerup(f)
fsm *f;
{
    f->flags |= LOWERUP;
    if (f->flags & AOPENDING)		/* Attempting Active-Open? */
	fsm_activeopen(f);		/* Try it now */
    else if (f->flags & POPENDING)	/* Attempting Passive-Open? */
	fsm_passiveopen(f);		/* Try it now */
}


/*
 * fsm_lowerdown - The lower layer is down.
 *
 * Cancel all timeouts and inform upper layers.
 */
void
  fsm_lowerdown(f)
fsm *f;
{
    f->flags &= ~LOWERUP;
    if (f->state == REQSENT ||		/* Timeout pending? */
	f->state == ACKRCVD ||
	f->state == ACKSENT ||
	f->state == TERMSENT)
	UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
    if (f->state == OPEN &&		/* OPEN? */
	f->callbacks->down)
	(*f->callbacks->down)(f);	/* Inform upper layers */
    f->state = CLOSED;
    if (f->callbacks->closed)
	(*f->callbacks->closed)(f);	/* Exit/restart/etc. */
}


/*
 * fsm_protreject - Peer doesn't speak this protocol.
 *
 * Pretend that the lower layer went down.
 */
void
  fsm_protreject(f)
fsm *f;
{
    fsm_lowerdown(f);
}


/*
 * fsm_input - Input packet.
 */
void
  fsm_input(f, inpacket, l)
fsm *f;
u_char *inpacket;
int l;
{
  u_char *inp, *outp;
  u_char code, id;
  int len;

  /*
   * Parse header (code, id and length).
   * If packet too short, drop it.
   */
  inp = inpacket;
  if (l < HEADERLEN) {
    FSMDEBUG((LOG_WARNING, "fsm_input(%x): Rcvd short header.", f->protocol))
    return;
  }
  GETCHAR(code, inp);
  GETCHAR(id, inp);
  GETSHORT(len, inp);
  if (len < HEADERLEN) {
    FSMDEBUG((LOG_INFO, "fsm_input(%x): Rcvd illegal length.",
	      f->protocol))
    return;
  }
  if (len > l) {
    FSMDEBUG((LOG_INFO, "fsm_input(%x): Rcvd short packet.",
	      f->protocol))
    return;
  }
  len -= HEADERLEN;		/* subtract header length */

  /*
   * Action depends on code.
   */
  switch (code) {
  case CONFREQ:
    FSMDEBUG((LOG_INFO, "fsm_rconfreq(%x): Rcvd id %d.",
	      f->protocol, id))

    if (f->state == TERMSENT)
      return;
    if (f->state == CLOSED) {
      fsm_sdata(f, TERMACK, id, NULL, 0);
      return;
    }
    if (f->state == OPEN && f->callbacks->down)
      (*f->callbacks->down)(f);	/* Inform upper layers */
    if (f->state == OPEN || f->state == LISTEN) {
      /* XXX Possibly need hold-down on OPEN? */
      fsm_sconfreq(f);		/* Send Configure-Request */
      TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
    }
    
    if (f->callbacks->reqci)	/* Check CI */
      code = (*f->callbacks->reqci)(f, inp, &len);
    else if (len)
      code = CONFREJ;		/* Reject all CI */
    
    len += HEADERLEN;	/* add header length back on */
    
    inp = inpacket;	              /* Reset to header */
    outp = outpacket_buf;	/* get pointer to output buffer */
    MAKEHEADER(outp, f->protocol); /* paste in DLL header */
    BCOPY(inp, outp, len);	/* copy input packet */
    PUTCHAR(code, outp);	/* put in the code, id, and length*/
    PUTCHAR(id, outp);
    PUTSHORT(len, outp);
    output(f->unit, outpacket_buf, len + DLLHEADERLEN);     /* send it out */ 
    
    if (code == CONFACK) {
      if (f->state == ACKRCVD) {
	UNTIMEOUT(fsm_timeout, (caddr_t) f); /* Cancel timeout */
	if (f->callbacks->up)
	  (*f->callbacks->up)(f); /* Inform upper layers */
	f->state = OPEN;
      }
      else
	f->state = ACKSENT;
    }
    else {
      if (f->state != ACKRCVD)
	f->state = REQSENT;
    }
    return;
    
  case CONFACK:
    fsm_rconfack(f, inp, id, len);
    break;
    
  case CONFNAK:
    fsm_rconfnak(f, inp, id, len);
    break;
    
  case CONFREJ:
    fsm_rconfrej(f, inp, id, len);
    break;
    
  case TERMREQ:
    fsm_rtermreq(f, id);
    break;
    
  case TERMACK:
    fsm_rtermack(f);
    break;
    
  case CODEREJ:
    fsm_rcoderej(f, inp, len);
    break;
    
  case PROTREJ:
    fsm_rprotrej(f, inp, len);
    break;
    
  case ECHOREQ:
    FSMDEBUG((LOG_INFO, "lcp: Echo-Request, Rcvd id %d", id));
    
    switch (f->state) {
    case CLOSED:
    case LISTEN:
      fsm_sdata(f, TERMACK, id, NULL, 0);
      break;
      
    case OPEN:
      inp = inpacket; /* Reset to header */
      outp = outpacket_buf;	/* get pointer to output buffer */
      MAKEHEADER(outp, f->protocol); /* add DLL header */
      len += HEADERLEN;		/* add header length */
      BCOPY(inp, outp, len);	/* copy input packet to output buffer */
      PUTCHAR(ECHOREP, outp);	/* set code to echo reply */
      PUTCHAR(id, outp);	/* add in id */
      PUTSHORT(len, outp);	/* and length */
      output(f->unit, outpacket_buf, len + DLLHEADERLEN); /* send it */
      return;
    }
    break;
    
  case ECHOREP:
  case DISCREQ:
    /* XXX Deliver to ECHOREQ sender? */
    break;
    
  default:
    fsm_sdata(f, CODEREJ, ++f->id, inpacket, len + HEADERLEN);
    break;
  }

}


/*
 * fsm_rconfack - Receive Configure-Ack.
 */
static void
  fsm_rconfack(f, inp, id, len)
fsm *f;
u_char *inp;
u_char id;
int len;
{
    FSMDEBUG((LOG_INFO, "fsm_rconfack(%x): Rcvd id %d.",
	      f->protocol, id))

    switch (f->state) {
      case LISTEN:
      case CLOSED:
	fsm_sdata(f, TERMACK, id, NULL, 0);
	break;

      case ACKRCVD:
      case REQSENT:
	if (id != f->reqid)		/* Expected id? */
	    break;			/* Nope, toss... */
	if (f->callbacks->ackci &&
	    (*f->callbacks->ackci)(f, inp, len)) /* Good ack? */
	    f->state = ACKRCVD;
	else
	    f->state = REQSENT;		/* Wait for timeout to retransmit */
	break;

      case ACKSENT:
	if (id != f->reqid)		/* Expected id? */
	    break;			/* Nope, toss... */
	if (f->callbacks->ackci &&
	    (*f->callbacks->ackci)(f, inp, len)) { /* Good ack? */
	    UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
	    if (f->callbacks->up)
		(*f->callbacks->up)(f);	/* Inform upper layers */
	    f->state = OPEN;
	}
	else
	    f->state = REQSENT;		/* Wait for timeout to retransmit */
	break;

      case OPEN:
	if (f->callbacks->down)
	    (*f->callbacks->down)(f);	/* Inform upper layers */
	f->state = CLOSED;		/* Only for a moment... */
	fsm_activeopen(f);		/* Restart */
	break;
    }
}


/*
 * fsm_rconfnak - Receive Configure-Nak.
 */
static void
  fsm_rconfnak(f, inp, id, len)
fsm *f;
u_char *inp;
u_char id;
int len;
{
    FSMDEBUG((LOG_INFO, "fsm_rconfnak(%x): Rcvd id %d.",
	      f->protocol, id))

    switch (f->state) {
      case LISTEN:
      case CLOSED:
	fsm_sdata(f, TERMACK, id, NULL, 0);
	break;

      case REQSENT:
      case ACKSENT:
	if (id != f->reqid)		/* Expected id? */
	    break;			/* Nope, toss... */
	if (++f->nakloops > f->maxnakloops) {
	    FSMDEBUG((LOG_INFO,
		      "fsm_rconfnak(%x): Possible CONFNAK loop!",
		      f->protocol))
	    break;			/* Break the loop */
	}
	UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
	if (f->callbacks->nakci)
	    (*f->callbacks->nakci)(f, inp, len);
	fsm_sconfreq(f);		/* Send Configure-Request */
	TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
	++f->retransmits;
	break;

      case ACKRCVD:
	f->state = REQSENT;		/* Wait for timeout to retransmit */
	break;

      case OPEN:
	if (f->callbacks->down)
	    (*f->callbacks->down)(f);	/* Inform upper layers */
	f->state = CLOSED;		/* Only for a moment... */
	fsm_activeopen(f);		/* Restart */
	break;
    }
}


/*
 * fsm_rconfrej - Receive Configure-Rej.
 */
static void
  fsm_rconfrej(f, inp, id, len)
fsm *f;
u_char *inp;
u_char id;
int len;
{
    FSMDEBUG((LOG_INFO, "fsm_rconfrej(%x): Rcvd id %d.",
	      f->protocol, id))

    switch (f->state) {
      case LISTEN:
      case CLOSED:
	fsm_sdata(f, TERMACK, id, NULL, 0);
	break;

      case REQSENT:
      case ACKSENT:
	if (id != f->reqid)		/* Expected id? */
	    break;			/* Nope, toss... */
	if (++f->nakloops > f->maxnakloops)
	    break;			/* Break the loop */
	UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
	if (f->callbacks->rejci)
	    (*f->callbacks->rejci)(f, inp, len);
	fsm_sconfreq(f);		/* Send Configure-Request */
	TIMEOUT(fsm_timeout, (caddr_t) f, f->timeouttime);
	++f->retransmits;
	break;

      case ACKRCVD:
	f->state = REQSENT;		/* Wait for timeout to retransmit */
	break;

      case OPEN:
	f->state = CLOSED;		/* Only for a moment... */
	fsm_activeopen(f);		/* Restart */
	break;
    }
}


/*
 * fsm_rtermreq - Receive Terminate-Req.
 */
static void
  fsm_rtermreq(f, id)
fsm *f;
u_char id;
{
    FSMDEBUG((LOG_INFO, "fsm_rtermreq(%x): Rcvd id %d.",
	      f->protocol, id))

    fsm_sdata(f, TERMACK, id, NULL, 0);
    switch (f->state) {
      case ACKRCVD:
      case ACKSENT:
	f->state = REQSENT;		/* Start over but keep trying */
	break;

      case OPEN:
	if (f->callbacks->down)
	    (*f->callbacks->down)(f);	/* Inform upper layers */
	f->state = CLOSED;
	if (f->callbacks->closed)
	    (*f->callbacks->closed)(f);	/* Exit/restart/etc. */
	break;
    }
}


/*
 * fsm_rtermack - Receive Terminate-Ack.
 */
static void
  fsm_rtermack(f)
fsm *f;
{
    FSMDEBUG((LOG_INFO, "fsm_rtermack(%x).", f->protocol))

    switch (f->state) {
      case OPEN:
	if (f->callbacks->down)
	    (*f->callbacks->down)(f);	/* Inform upper layers */
	f->state = CLOSED;
	if (f->callbacks->closed)
	    (*f->callbacks->closed)(f);	/* Exit/restart/etc. */
	break;

      case TERMSENT:
	UNTIMEOUT(fsm_timeout, (caddr_t) f);	/* Cancel timeout */
	f->state = CLOSED;
	if (f->callbacks->closed)
	    (*f->callbacks->closed)(f);	/* Exit/restart/etc. */
	break;
    }
}


/*
 * fsm_rcoderej - Receive an Code-Reject.
 */
static void
  fsm_rcoderej(f, inp, len)
fsm *f;
u_char *inp;
int len;
{
    u_char code;

    FSMDEBUG((LOG_INFO, "fsm_rcoderej(%x).", f->protocol))

    if (len < sizeof (u_char)) {
	FSMDEBUG((LOG_INFO,
		  "fsm_rcoderej: Rcvd short Code-Reject packet!"))
	return;
    }
    GETCHAR(code, inp);
    FSMDEBUG((LOG_INFO,
	      "fsm_rcoderej: Rcvd Code-Reject for code %d!",
	      code))
}


/*
 * fsm_rprotrej - Receive an Protocol-Reject.
 *
 * Figure out which protocol is rejected and inform it.
 */
static void
  fsm_rprotrej(f, inp, len)
fsm *f;
u_char *inp;
int len;
{
    u_short prot;

    FSMDEBUG((LOG_INFO, "fsm_rprotrej."))

    if (len < sizeof (u_short)) {
	FSMDEBUG((LOG_INFO,
		  "fsm_rprotrej: Rcvd short Protocol-Reject packet!"))
	return;
    }
    if (f->protocol != LCP) {		/* Only valid for LCP */
	FSMDEBUG((LOG_INFO,
		  "fsm_rprotrej: Rcvd non-LCP Protocol-Reject!"))
	return;
    }

    GETSHORT(prot, inp);

    FSMDEBUG((LOG_INFO,
	      "fsm_rprotrej: Rcvd Protocol-Reject packet for %x!",
	      prot))
    DEMUXPROTREJ(f->unit, prot);	/* Inform protocol */
}


/*
 * fsm_sconfreq - Send a Configure-Request.
 */
static void
  fsm_sconfreq(f)
fsm *f;
{
    u_char *outp;
    int outlen;

    outlen = HEADERLEN + (f->callbacks->cilen ? (*f->callbacks->cilen)(f) : 0);
    /* XXX Adjust outlen to MTU */
    outp = outpacket_buf;
    MAKEHEADER(outp, f->protocol);

    PUTCHAR(CONFREQ, outp);
    PUTCHAR(f->reqid = ++f->id, outp);
    PUTSHORT(outlen, outp);
    if (f->callbacks->cilen && f->callbacks->addci)
	(*f->callbacks->addci)(f, outp);
    output(f->unit, outpacket_buf, outlen + DLLHEADERLEN);

    FSMDEBUG((LOG_INFO, "%s: sending Configure-Request, id %d",
	      proto_name(f->protocol), f->reqid))
}


/*
 * fsm_sdata - Send some data.
 *
 * Used for Terminate-Request, Terminate-Ack, Code-Reject, Protocol-Reject,
 * Echo-Request, and Discard-Request.
 */
void
  fsm_sdata(f, code, id, data, datalen)
fsm *f;
u_char code, id;
u_char *data;
int datalen;
{
    u_char *outp;
    int outlen;

    /* Adjust length to be smaller than MTU */
    if (datalen > MTU - HEADERLEN)
	datalen = MTU - HEADERLEN;
    outlen = datalen + HEADERLEN;
    outp = outpacket_buf;
    MAKEHEADER(outp, f->protocol);
    PUTCHAR(code, outp);
    PUTCHAR(id, outp);
    PUTSHORT(outlen, outp);
    if (datalen)
	BCOPY(data, outp, datalen);
    output(f->unit, outpacket_buf, outlen + DLLHEADERLEN);

    FSMDEBUG((LOG_INFO, "fsm_sdata(%x): Sent code %d, id %d.",
	      f->protocol, code, id))
}
