/*
 * upap.c - User/Password Authentication Protocol.
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
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>

#ifdef STREAMS
#include <sys/socket.h>
#include <net/if.h>
#include <sys/stream.h>
#endif

#include <net/ppp.h>
#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "upap.h"
#include "chap.h"
#include "ipcp.h"


upap_state upap[NPPP];		/* UPAP state; one for each unit */


static void upap_timeout __ARGS((caddr_t));
static void upap_rauth __ARGS((upap_state *, u_char *, int, int));
static void upap_rauthack __ARGS((upap_state *, u_char *, int, int));
static void upap_rauthnak __ARGS((upap_state *, u_char *, int, int));
static void upap_sauth __ARGS((upap_state *));
static void upap_sresp __ARGS((upap_state *, int, int, u_char *, int));


/*
 * upap_init - Initialize a UPAP unit.
 */
void
  upap_init(unit)
int unit;
{
    upap_state *u = &upap[unit];

    u->us_unit = unit;
    u->us_user = NULL;
    u->us_userlen = 0;
    u->us_passwd = NULL;
    u->us_passwdlen = 0;
    u->us_clientstate = UPAPCS_CLOSED;
    u->us_serverstate = UPAPSS_CLOSED;
    u->us_flags = 0;
    u->us_id = 0;
    u->us_timeouttime = UPAP_DEFTIMEOUT;
}


/*
 * upap_authwithpeer - Authenticate us with our peer (start client).
 *
 * Set new state and send authenticate's.
 */
void
  upap_authwithpeer(unit)
int unit;
{
    upap_state *u = &upap[unit];

    u->us_flags &= ~UPAPF_AWPPENDING;	/* Clear pending flag */

    /* Protect against programming errors that compromise security */
    if (u->us_serverstate != UPAPSS_CLOSED ||
	u->us_flags & UPAPF_APPENDING) {
	UPAPDEBUG((LOG_WARNING,
		   "upap_authwithpeer: upap_authpeer already called!"))
	return;
    }

    /* Already authenticat{ed,ing}? */
    if (u->us_clientstate == UPAPCS_AUTHSENT ||
	u->us_clientstate == UPAPCS_OPEN)
	return;

    /* Lower layer up? */
    if (!(u->us_flags & UPAPF_LOWERUP)) {
	u->us_flags |= UPAPF_AWPPENDING; /* Wait */
	return;
    }

    /* User/passwd values valid? */
    if (!(u->us_flags & UPAPF_UPVALID)) {
	GETUSERPASSWD(unit);		/* Start getting user and passwd */
	if (!(u->us_flags & UPAPF_UPVALID)) {
	    u->us_flags |= UPAPF_UPPENDING;	/* Wait */
	    return;
	}
    }

    upap_sauth(u);			/* Start protocol */
/*    TIMEOUT(upap_timeout, (caddr_t) u, u->us_timeouttime);*/
    u->us_clientstate = UPAPCS_AUTHSENT;
    u->us_retransmits = 0;
}


/*
 * upap_authpeer - Authenticate our peer (start server).
 *
 * Set new state.
 */
void
  upap_authpeer(unit)
int unit;
{
    upap_state *u = &upap[unit];

    u->us_flags &= ~UPAPF_APPENDING;	/* Clear pending flag */

    /* Already authenticat{ed,ing}? */
    if (u->us_serverstate == UPAPSS_LISTEN ||
	u->us_serverstate == UPAPSS_OPEN)
	return;

    /* Lower layer up? */
    if (!(u->us_flags & UPAPF_LOWERUP)) {
	u->us_flags |= UPAPF_APPENDING;	/* Wait for desired event */
	return;
    }
    u->us_serverstate = UPAPSS_LISTEN;
}


/*
 * upap_timeout - Timeout expired.
 */
static void
  upap_timeout(arg)
caddr_t arg;
{
  upap_state *u = (upap_state *) arg;

    if (u->us_clientstate != UPAPCS_AUTHSENT)
	return;

    /* XXX Print warning after many retransmits? */

    upap_sauth(u);			/* Send Configure-Request */
    TIMEOUT(upap_timeout, (caddr_t) u, u->us_timeouttime);
    ++u->us_retransmits;
}


/*
 * upap_lowerup - The lower layer is up.
 *
 * Start authenticating if pending.
 */
void
  upap_lowerup(unit)
int unit;
{
    upap_state *u = &upap[unit];

    u->us_flags |= UPAPF_LOWERUP;
    if (u->us_flags & UPAPF_AWPPENDING)	/* Attempting authwithpeer? */
	upap_authwithpeer(unit);	/* Try it now */
    if (u->us_flags & UPAPF_APPENDING)	/* Attempting authpeer? */
	upap_authpeer(unit);		/* Try it now */
}


/*
 * upap_lowerdown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
void
  upap_lowerdown(unit)
int unit;
{
    upap_state *u = &upap[unit];

    u->us_flags &= ~UPAPF_LOWERUP;	/* XXX UPAP_UPVALID? */

    if (u->us_clientstate == UPAPCS_AUTHSENT) /* Timeout pending? */
	UNTIMEOUT(upap_timeout, (caddr_t) u);	/* Cancel timeout */

    if (u->us_serverstate == UPAPSS_OPEN) /* User logged in? */
	LOGOUT(unit);
    u->us_clientstate = UPAPCS_CLOSED;
    u->us_serverstate = UPAPSS_CLOSED;
}


/*
 * upap_protrej - Peer doesn't speak this protocol.
 *
 * This shouldn't happen.  In any case, pretend lower layer went down.
 */
void
  upap_protrej(unit)
int unit;
{
    upap_lowerdown(unit);
}


/*
 * upap_input - Input UPAP packet.
 */
void
  upap_input(unit, inpacket, l)
int unit;
u_char *inpacket;
int l;
{
    upap_state *u = &upap[unit];
    u_char *inp;
    u_char code, id;
    int len;

    /*
     * Parse header (code, id and length).
     * If packet too short, drop it.
     */
    inp = inpacket;
    if (l < UPAP_HEADERLEN) {
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd short header."))
	return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < UPAP_HEADERLEN) {
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd illegal length."))
	return;
    }
    if (len > l) {
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd short packet."))
	return;
    }
    len -= UPAP_HEADERLEN;

    /*
     * Action depends on code.
     */
    switch (code) {
      case UPAP_AUTH:
	upap_rauth(u, inp, id, len);
	break;

      case UPAP_AUTHACK:
	upap_rauthack(u, inp, id, len);
	break;

      case UPAP_AUTHNAK:
	upap_rauthnak(u, inp, id, len);
	break;

      default:				/* XXX Need code reject */
	break;
    }
}


/*
 * upap_rauth - Receive Authenticate.
 */
static void
  upap_rauth(u, inp, id, len)
upap_state *u;
u_char *inp;
u_char id;
int len;
{
    u_char ruserlen, rpasswdlen;
    u_char *ruser, *rpasswd;
    u_char retcode;
    u_char *msg;
    int msglen;

    UPAPDEBUG((LOG_INFO, "upap_rauth: Rcvd id %d.", id))
    if (u->us_serverstate != UPAPSS_LISTEN) /* XXX Reset connection? */
	return;

    /*
     * Parse user/passwd.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."))
	return;
    }
    GETCHAR(ruserlen, inp);
    len -= sizeof (u_char) + ruserlen + sizeof (u_char);;
    if (len < 0) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."))
	return;
    }
    ruser = inp;
    INCPTR(ruserlen, inp);
    GETCHAR(rpasswdlen, inp);
    if (len < rpasswdlen) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."))
	return;
    }
    rpasswd = inp;

    retcode = LOGIN(u->us_unit, (char *) ruser, (int) ruserlen, (char *) rpasswd,
		    (int) rpasswdlen, (char **) &msg, &msglen);

    upap_sresp(u, retcode, id, msg, msglen);

  /* only crank up IPCP when either we aren't doing CHAP, or if we are, */
  /* that it is in open state */

    if (retcode == UPAP_AUTHACK) {
	u->us_serverstate = UPAPSS_OPEN;
	if (!lcp_hisoptions[u->us_unit].neg_chap ||
	    (lcp_hisoptions[u->us_unit].neg_chap &&
	     chap[u->us_unit].serverstate == CHAPSS_OPEN))
	  ipcp_activeopen(u->us_unit);	/* Start IPCP */
    }
}


/*
 * upap_rauthack - Receive Authenticate-Ack.
 */
static void
  upap_rauthack(u, inp, id, len)
upap_state *u;
u_char *inp;
u_char id;
int len;
{
    u_char msglen;
    u_char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthack: Rcvd id %d.", id))
    if (u->us_clientstate != UPAPCS_AUTHSENT) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet."))
	return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
	UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet."))
	return;
    }
    msg = inp;
    PRINTMSG(msg, msglen);

    u->us_clientstate = UPAPCS_OPEN;

  /* only crank up IPCP when either we aren't doing CHAP, or if we are, */
  /* that it is in open state */

    if (!lcp_gotoptions[u->us_unit].neg_chap ||
	(lcp_gotoptions[u->us_unit].neg_chap &&
	chap[u->us_unit].clientstate == CHAPCS_OPEN)) 
      ipcp_activeopen(u->us_unit);	/* Start IPCP */
}


/*
 * upap_rauthnak - Receive Authenticate-Nakk.
 */
static void
  upap_rauthnak(u, inp, id, len)
upap_state *u;
u_char *inp;
u_char id;
int len;
{
    u_char msglen;
    u_char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthnak: Rcvd id %d.", id))
    if (u->us_clientstate != UPAPCS_AUTHSENT) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet."))
	return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
	UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet."))
	return;
    }
    msg = inp;
    PRINTMSG(msg, msglen);

    u->us_flags &= ~UPAPF_UPVALID;	/* Clear valid flag */
    u->us_clientstate = UPAPCS_CLOSED;	/* Pretend for a moment */
    upap_authwithpeer(u->us_unit);	/* Restart */
}


/*
 * upap_sauth - Send an Authenticate.
 */
static void
  upap_sauth(u)
upap_state *u;
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + 2 * sizeof (u_char) +
	u->us_userlen + u->us_passwdlen;
    outp = outpacket_buf;
    
    MAKEHEADER(outp, UPAP);

    PUTCHAR(UPAP_AUTH, outp);
    PUTCHAR(++u->us_id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(u->us_userlen, outp);
    BCOPY(u->us_user, outp, u->us_userlen);
    INCPTR(u->us_userlen, outp);
    PUTCHAR(u->us_passwdlen, outp);
    BCOPY(u->us_passwd, outp, u->us_passwdlen);
    output(u->us_unit, outpacket_buf, outlen + DLLHEADERLEN);

    UPAPDEBUG((LOG_INFO, "upap_sauth: Sent id %d.", u->us_id))
}


/*
 * upap_sresp - Send a response (ack or nak).
 */
static void
  upap_sresp(u, code, id, msg, msglen)
upap_state *u;
u_char code, id;
u_char *msg;
int msglen;
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + sizeof (u_char) + msglen;
    outp = outpacket_buf;
    MAKEHEADER(outp, UPAP);

    PUTCHAR(code, outp);
    PUTCHAR(id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(msglen, outp);
    BCOPY(msg, outp, msglen);
    output(u->us_unit, outpacket_buf, outlen + DLLHEADERLEN);

    UPAPDEBUG((LOG_INFO, "upap_sresp: Sent code %d, id %d.", code, id))
}
