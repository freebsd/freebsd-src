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

#ifndef lint
static char rcsid[] = "$Id: upap.c,v 1.3 1994/03/30 09:38:20 jkh Exp $";
#endif

/*
 * TODO:
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>

#include "ppp.h"
#include "pppd.h"
#include "upap.h"


upap_state upap[_NPPP];		/* UPAP state; one for each unit */


static void upap_timeout __ARGS((caddr_t));
static void upap_rauthreq __ARGS((upap_state *, u_char *, int, int));
static void upap_rauthack __ARGS((upap_state *, u_char *, int, int));
static void upap_rauthnak __ARGS((upap_state *, u_char *, int, int));
static void upap_sauthreq __ARGS((upap_state *));
static void upap_sresp __ARGS((upap_state *, int, int, char *, int));


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
    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
    u->us_id = 0;
    u->us_timeouttime = UPAP_DEFTIMEOUT;
    u->us_maxtransmits = 10;
}


/*
 * upap_authwithpeer - Authenticate us with our peer (start client).
 *
 * Set new state and send authenticate's.
 */
void
upap_authwithpeer(unit, user, password)
    int unit;
    char *user, *password;
{
    upap_state *u = &upap[unit];

    /* Save the username and password we're given */
    u->us_user = user;
    u->us_userlen = strlen(user);
    u->us_passwd = password;
    u->us_passwdlen = strlen(password);
    u->us_transmits = 0;

    /* Lower layer up yet? */
    if (u->us_clientstate == UPAPCS_INITIAL ||
	u->us_clientstate == UPAPCS_PENDING) {
	u->us_clientstate = UPAPCS_PENDING;
	return;
    }

    upap_sauthreq(u);			/* Start protocol */
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

    /* Lower layer up yet? */
    if (u->us_serverstate == UPAPSS_INITIAL ||
	u->us_serverstate == UPAPSS_PENDING) {
	u->us_serverstate = UPAPSS_PENDING;
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

    if (u->us_clientstate != UPAPCS_AUTHREQ)
	return;

    if (u->us_transmits >= u->us_maxtransmits) {
	/* give up in disgust */
	syslog(LOG_ERR, "No response to PAP authenticate-requests");
	u->us_clientstate = UPAPCS_BADAUTH;
	auth_withpeer_fail(u->us_unit, UPAP);
	return;
    }

    upap_sauthreq(u);		/* Send Authenticate-Request */
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

    if (u->us_clientstate == UPAPCS_INITIAL)
	u->us_clientstate = UPAPCS_CLOSED;
    else if (u->us_clientstate == UPAPCS_PENDING) {
	upap_sauthreq(u);	/* send an auth-request */
    }

    if (u->us_serverstate == UPAPSS_INITIAL)
	u->us_serverstate = UPAPSS_CLOSED;
    else if (u->us_serverstate == UPAPSS_PENDING)
	u->us_serverstate = UPAPSS_LISTEN;
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

    if (u->us_clientstate == UPAPCS_AUTHREQ) /* Timeout pending? */
	UNTIMEOUT(upap_timeout, (caddr_t) u);	/* Cancel timeout */

    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
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
    upap_state *u = &upap[unit];

    if (u->us_clientstate == UPAPCS_AUTHREQ) {
	syslog(LOG_ERR, "PAP authentication failed due to protocol-reject");
	auth_withpeer_fail(unit, UPAP);
    }
    if (u->us_serverstate == UPAPSS_LISTEN) {
	syslog(LOG_ERR, "PAP authentication of peer failed (protocol-reject)");
	auth_peer_fail(unit, UPAP);
    }
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
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd short header."));
	return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < UPAP_HEADERLEN) {
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd illegal length."));
	return;
    }
    if (len > l) {
	UPAPDEBUG((LOG_INFO, "upap_input: rcvd short packet."));
	return;
    }
    len -= UPAP_HEADERLEN;

    /*
     * Action depends on code.
     */
    switch (code) {
    case UPAP_AUTHREQ:
	upap_rauthreq(u, inp, id, len);
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
upap_rauthreq(u, inp, id, len)
    upap_state *u;
    u_char *inp;
    int id;
    int len;
{
    u_char ruserlen, rpasswdlen;
    char *ruser, *rpasswd;
    int retcode;
    char *msg;
    int msglen;

    UPAPDEBUG((LOG_INFO, "upap_rauth: Rcvd id %d.", id));

    if (u->us_serverstate < UPAPSS_LISTEN)
	return;

    /*
     * If we receive a duplicate authenticate-request, we are
     * supposed to return the same status as for the first request.
     */
    if (u->us_serverstate == UPAPSS_OPEN) {
	upap_sresp(u, UPAP_AUTHACK, id, "", 0);	/* return auth-ack */
	return;
    }
    if (u->us_serverstate == UPAPSS_BADAUTH) {
	upap_sresp(u, UPAP_AUTHNAK, id, "", 0);	/* return auth-nak */
	return;
    }

    /*
     * Parse user/passwd.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."));
	return;
    }
    GETCHAR(ruserlen, inp);
    len -= sizeof (u_char) + ruserlen + sizeof (u_char);;
    if (len < 0) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."));
	return;
    }
    ruser = (char *) inp;
    INCPTR(ruserlen, inp);
    GETCHAR(rpasswdlen, inp);
    if (len < rpasswdlen) {
	UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet."));
	return;
    }
    rpasswd = (char *) inp;

    /*
     * Check the username and password given.
     */
    retcode = check_passwd(u->us_unit, ruser, ruserlen, rpasswd,
			   rpasswdlen, &msg, &msglen);

    upap_sresp(u, retcode, id, msg, msglen);

    if (retcode == UPAP_AUTHACK) {
	u->us_serverstate = UPAPSS_OPEN;
	auth_peer_success(u->us_unit, UPAP);
    } else {
	u->us_serverstate = UPAPSS_BADAUTH;
	auth_peer_fail(u->us_unit, UPAP);
    }
}


/*
 * upap_rauthack - Receive Authenticate-Ack.
 */
static void
upap_rauthack(u, inp, id, len)
    upap_state *u;
    u_char *inp;
    int id;
    int len;
{
    u_char msglen;
    char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthack: Rcvd id %d.", id));
    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet."));
	return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
	UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet."));
	return;
    }
    msg = (char *) inp;
    PRINTMSG(msg, msglen);

    u->us_clientstate = UPAPCS_OPEN;

    auth_withpeer_success(u->us_unit, UPAP);
}


/*
 * upap_rauthnak - Receive Authenticate-Nakk.
 */
static void
upap_rauthnak(u, inp, id, len)
    upap_state *u;
    u_char *inp;
    int id;
    int len;
{
    u_char msglen;
    char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthnak: Rcvd id %d.", id));
    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
	UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet."));
	return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
	UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet."));
	return;
    }
    msg = (char *) inp;
    PRINTMSG(msg, msglen);

    u->us_clientstate = UPAPCS_BADAUTH;

    syslog(LOG_ERR, "PAP authentication failed");
    auth_withpeer_fail(u->us_unit, UPAP);
}


/*
 * upap_sauthreq - Send an Authenticate-Request.
 */
static void
upap_sauthreq(u)
    upap_state *u;
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + 2 * sizeof (u_char) +
	u->us_userlen + u->us_passwdlen;
    outp = outpacket_buf;
    
    MAKEHEADER(outp, UPAP);

    PUTCHAR(UPAP_AUTHREQ, outp);
    PUTCHAR(++u->us_id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(u->us_userlen, outp);
    BCOPY(u->us_user, outp, u->us_userlen);
    INCPTR(u->us_userlen, outp);
    PUTCHAR(u->us_passwdlen, outp);
    BCOPY(u->us_passwd, outp, u->us_passwdlen);

    output(u->us_unit, outpacket_buf, outlen + DLLHEADERLEN);

    UPAPDEBUG((LOG_INFO, "upap_sauth: Sent id %d.", u->us_id));

    TIMEOUT(upap_timeout, (caddr_t) u, u->us_timeouttime);
    ++u->us_transmits;
    u->us_clientstate = UPAPCS_AUTHREQ;
}


/*
 * upap_sresp - Send a response (ack or nak).
 */
static void
upap_sresp(u, code, id, msg, msglen)
    upap_state *u;
    u_char code, id;
    char *msg;
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

    UPAPDEBUG((LOG_INFO, "upap_sresp: Sent code %d, id %d.", code, id));
}
