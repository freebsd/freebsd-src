/*
 * chap.c - Crytographic Handshake Authentication Protocol.
 *
 * Copyright (c) 1991 Gregory M. Christy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Gregory M. Christy.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
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
#include "chap.h"
#include "upap.h"
#include "ipcp.h"
#include "md5.h"

chap_state chap[NPPP];		/* CHAP state; one for each unit */

static void ChapTimeout __ARGS((caddr_t));
static void ChapReceiveChallenge __ARGS((chap_state *, u_char *, int, int));
static void ChapReceiveResponse __ARGS((chap_state *, u_char *, int, int));
static void ChapReceiveSuccess __ARGS((chap_state *, u_char *, int, int));
static void ChapReceiveFailure __ARGS((chap_state *, u_char *, int, int));
static void ChapSendStatus __ARGS((chap_state *, int, int,
				   u_char *, int));
static void ChapSendChallenge __ARGS((chap_state *));
static void ChapSendResponse __ARGS((chap_state *, int, u_char *, int));
static void ChapGenChallenge __ARGS((int, u_char *));

extern double drand48 __ARGS((void));
extern void srand48 __ARGS((long));

/*
 * ChapInit - Initialize a CHAP unit.
 */
void
  ChapInit(unit)
int unit;
{
  chap_state *cstate = &chap[unit];

  cstate->unit = unit;
  cstate->chal_str[0] = '\000';
  cstate->chal_len = 0;
  cstate->clientstate = CHAPCS_CLOSED;
  cstate->serverstate = CHAPSS_CLOSED;
  cstate->flags = 0;
  cstate->id = 0;
  cstate->timeouttime = CHAP_DEFTIMEOUT;
  cstate->retransmits = 0;
  srand48((long) time(NULL));	/* joggle random number generator */
}


/*
 * ChapAuthWithPeer - Authenticate us with our peer (start client).
 *
 */
void
  ChapAuthWithPeer(unit)
int unit;
{
  chap_state *cstate = &chap[unit];
  
  cstate->flags &= ~CHAPF_AWPPENDING;	/* Clear pending flag */
  
  /* Protect against programming errors that compromise security */
  if (cstate->serverstate != CHAPSS_CLOSED ||
      cstate->flags & CHAPF_APPENDING) {
    CHAPDEBUG((LOG_INFO,
	       "ChapAuthWithPeer: we were called already!"))
    return;
  }
  
  if (cstate->clientstate == CHAPCS_CHALLENGE_SENT ||  /* should we be here? */
      cstate->clientstate == CHAPCS_OPEN)
    return;
  
  /* Lower layer up? */
  if (!(cstate->flags & CHAPF_LOWERUP)) {
    cstate->flags |= CHAPF_AWPPENDING; /* Nah, Wait */
    return;
  }
  ChapSendChallenge(cstate);		/* crank it up dude! */
  TIMEOUT(ChapTimeout, (caddr_t) cstate, cstate->timeouttime); 
					/* set-up timeout */
  cstate->clientstate = CHAPCS_CHALLENGE_SENT; /* update state */
  cstate->retransmits = 0;
}


/*
 * ChapAuthPeer - Authenticate our peer (start server).
 */
void
  ChapAuthPeer(unit)
int unit;
{
  chap_state *cstate = &chap[unit];
  
  cstate->flags &= ~CHAPF_APPENDING;	/* Clear pending flag */
  
  /* Already authenticat{ed,ing}? */
  if (cstate->serverstate == CHAPSS_LISTEN ||
      cstate->serverstate == CHAPSS_OPEN)
    return;
  
  /* Lower layer up? */
  if (!(cstate->flags & CHAPF_LOWERUP)) {
    cstate->flags |= CHAPF_APPENDING;	/* Wait for desired event */
    return;
  }
  cstate->serverstate = CHAPSS_LISTEN;
}


/*
 * ChapTimeout - Timeout expired.
 */
static void
  ChapTimeout(arg)
caddr_t arg;
{
  chap_state *cstate = (chap_state *) arg;
  
  /* if we aren't sending challenges, don't worry.  then again we */
  /* probably shouldn't be here either */
  if (cstate->clientstate != CHAPCS_CHALLENGE_SENT)
    return;
  
  ChapSendChallenge(cstate);			/* Send challenge */
  TIMEOUT(ChapTimeout, (caddr_t) cstate, cstate->timeouttime);
  ++cstate->retransmits;
}


/*
 * ChapLowerUp - The lower layer is up.
 *
 * Start up if we have pending requests.
 */
void
  ChapLowerUp(unit)
int unit;
{
  chap_state *cstate = &chap[unit];
  
  cstate->flags |= CHAPF_LOWERUP;
  if (cstate->flags & CHAPF_AWPPENDING)	/* were we attempting authwithpeer? */
    ChapAuthWithPeer(unit);	/* Try it now */
  if (cstate->flags & CHAPF_APPENDING)	/* or authpeer? */
    ChapAuthPeer(unit);
}


/*
 * ChapLowerDown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
void
  ChapLowerDown(unit)
int unit;
{
  chap_state *cstate = &chap[unit];
  
  cstate->flags &= ~CHAPF_LOWERUP;
  
  if (cstate->clientstate == CHAPCS_CHALLENGE_SENT) /* Timeout pending? */
    UNTIMEOUT(ChapTimeout, (caddr_t) cstate);	/* Cancel timeout */
  
  if (cstate->serverstate == CHAPSS_OPEN) /* have we successfully authed? */
    LOGOUT(unit);
  cstate->clientstate = CHAPCS_CLOSED;
  cstate->serverstate = CHAPSS_CLOSED;
}


/*
 * ChapProtocolReject - Peer doesn't grok CHAP.
 */
void
  ChapProtocolReject(unit)
int unit;
{
  ChapLowerDown(unit);		/* shutdown chap */


/* Note: should we bail here if chap is required? */
}


/*
 * ChapInput - Input CHAP packet.
 */
void
  ChapInput(unit, inpacket, packet_len)
int unit;
u_char *inpacket;
int packet_len;
{
  chap_state *cstate = &chap[unit];
  u_char *inp;
  u_char code, id;
  int len;
  
  /*
   * Parse header (code, id and length).
   * If packet too short, drop it.
   */
  inp = inpacket;
  if (packet_len < CHAP_HEADERLEN) {
    CHAPDEBUG((LOG_INFO, "ChapInput: rcvd short header."))
    return;
  }
  GETCHAR(code, inp);
  GETCHAR(id, inp);
  GETSHORT(len, inp);
  if (len < CHAP_HEADERLEN) {
    CHAPDEBUG((LOG_INFO, "ChapInput: rcvd illegal length."))
    return;
  }
  if (len > packet_len) {
    CHAPDEBUG((LOG_INFO, "ChapInput: rcvd short packet."))
    return;
  }
  len -= CHAP_HEADERLEN;
  
  /*
   * Action depends on code.
   */
  switch (code) {
  case CHAP_CHALLENGE:
    ChapReceiveChallenge(cstate, inp, id, len);
    break;
    
  case CHAP_RESPONSE:
    ChapReceiveResponse(cstate, inp, id, len);
    break;
    
  case CHAP_FAILURE:
    ChapReceiveFailure(cstate, inp, id, len);
    break;

  case CHAP_SUCCESS:
    ChapReceiveSuccess(cstate, inp, id, len);
    break;

  default:				/* Need code reject? */
    syslog(LOG_WARNING, "Unknown CHAP code (%d) received.", code);
    break;
  }
}


/*
 * ChapReceiveChallenge - Receive Challenge.
 */
static void
  ChapReceiveChallenge(cstate, inp, id, len)
chap_state *cstate;
u_char *inp;
int id;
int len;
{
  u_char rchallenge_len;
  u_char *rchallenge;
  u_char secret[MAX_SECRET_LEN];
  int secret_len;
  u_char rhostname[256];
  u_char buf[256];
  MD5_CTX mdContext;
 
  CHAPDEBUG((LOG_INFO, "ChapReceiveChallenge: Rcvd id %d.", id))
  if (cstate->serverstate != CHAPSS_LISTEN) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveChallenge: received challenge but not in listen state"))
    return;
  }
  
  if (len < 2) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveChallenge: rcvd short packet."))
    return;
  }
  GETCHAR(rchallenge_len, inp);
  len -= sizeof (u_char) + rchallenge_len ;
  if (len < 0) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveChallenge: rcvd short packet."))
    return;
  }
  rchallenge = inp;
  INCPTR(rchallenge_len, inp);

  BCOPY(inp, rhostname, len);
  rhostname[len] = '\000';

  CHAPDEBUG((LOG_INFO, "ChapReceiveChallenge: received name field: %s",
	     rhostname))
  GETSECRET(rhostname, secret, &secret_len);/* get secret for specified host */

  BCOPY(rchallenge, buf, rchallenge_len); /* copy challenge into buffer */
  BCOPY(secret, buf + rchallenge_len, secret_len); /* append secret */

  /*  generate MD based on negotiated type */

  switch (lcp_hisoptions[cstate->unit].chap_mdtype) { 

  case CHAP_DIGEST_MD5:		/* only MD5 is defined for now */
    MD5Init(&mdContext);
    MD5Update(&mdContext, buf, rchallenge_len + secret_len);
    MD5Final(&mdContext); 
    ChapSendResponse(cstate, id, &mdContext.digest[0], MD5_SIGNATURE_SIZE);
    break;

  default:
    CHAPDEBUG((LOG_INFO, "unknown digest type %d",
	       lcp_hisoptions[cstate->unit].chap_mdtype))
  }

}


/*
 * ChapReceiveResponse - Receive and process response.
 */
static void
  ChapReceiveResponse(cstate, inp, id, len)
chap_state *cstate;
u_char *inp;
int id;
int len;
{
  u_char *remmd, remmd_len;
  u_char secret[MAX_SECRET_LEN];
  int secret_len;
  u_char chal_len = cstate->chal_len;
  u_char code;
  u_char rhostname[256];
  u_char buf[256];
  MD5_CTX mdContext;
  u_char msg[256], msglen;

  CHAPDEBUG((LOG_INFO, "ChapReceiveResponse: Rcvd id %d.", id))

/* sanity check */
  if (cstate->clientstate != CHAPCS_CHALLENGE_SENT) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveResponse: received response but did not send a challenge"))
    return;
  }
  
  if (len < 2) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveResponse: rcvd short packet."))
    return;
  }
  GETCHAR(remmd_len, inp);		/* get length of MD */
  len -= sizeof (u_char) + remmd_len ;

  if (len < 0) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveResponse: rcvd short packet."))
    return;
  }

  remmd = inp;			/* get pointer to MD */
  INCPTR(remmd_len, inp);

  BCOPY(inp, rhostname, len);
  rhostname[len] = '\000';

  CHAPDEBUG((LOG_INFO, "ChapReceiveResponse: received name field: %s",
	     rhostname))

  GETSECRET(rhostname, secret, &secret_len);/* get secret for specified host */

  BCOPY(cstate->chal_str, buf, chal_len); /* copy challenge */
						  /* into buffer */ 
  BCOPY(secret, buf + chal_len, secret_len); /* append secret */

  /*  generate MD based on negotiated type */

  switch (lcp_gotoptions[cstate->unit].chap_mdtype) { 

  case CHAP_DIGEST_MD5:		/* only MD5 is defined for now */
    MD5Init(&mdContext);
    MD5Update(&mdContext, buf, chal_len + secret_len);
    MD5Final(&mdContext); 

    /* compare local and remote MDs and send the appropriate status */

    if (bcmp (&mdContext.digest[0], remmd, MD5_SIGNATURE_SIZE)) 
      code = CHAP_FAILURE;	/* they ain't the same */
    else
      code = CHAP_SUCCESS;	/* they are the same! */
    break;

    default:
      CHAPDEBUG((LOG_INFO, "unknown digest type %d",
		 lcp_gotoptions[cstate->unit].chap_mdtype))
    }
    if (code == CHAP_SUCCESS)
      sprintf((char *)msg, "Welcome to %s.", hostname);
    else
      sprintf((char *)msg, "I don't like you.  Go 'way.");
    msglen = strlen(msg);
    ChapSendStatus(cstate, code, id, msg, msglen);
  
  /* only crank up IPCP when either we aren't doing PAP, or if we are, */
  /* that it is in open state */

    if (code == CHAP_SUCCESS) {
      cstate->serverstate = CHAPSS_OPEN;
      if (!lcp_hisoptions[cstate->unit].neg_upap ||
	  (lcp_hisoptions[cstate->unit].neg_upap &&
	   upap[cstate->unit].us_serverstate == UPAPSS_OPEN ))
	ipcp_activeopen(cstate->unit);	/* Start IPCP */
    }
}
/*
 * ChapReceiveSuccess - Receive Success
 */
/* ARGSUSED */
static void
  ChapReceiveSuccess(cstate, inp, id, len)
chap_state *cstate;
u_char *inp;
u_char id;
int len;
{
  u_char msglen;
  u_char *msg;

  CHAPDEBUG((LOG_INFO, "ChapReceiveSuccess: Rcvd id %d.", id))

  if (cstate->clientstate != CHAPCS_CHALLENGE_SENT) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveSuccess: received success, but did not send a challenge."))
    return;
  }

  /*
   * Parse message.
   */
  if (len < sizeof (u_char)) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveSuccess: rcvd short packet."))
    return;
  }
  GETCHAR(msglen, inp);
  len -= sizeof (u_char);
  if (len < msglen) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveSuccess: rcvd short packet."))
    return;
  }
  msg = inp;
  PRINTMSG(msg, msglen);
  
  cstate->clientstate = CHAPCS_OPEN;

  /* only crank up IPCP when either we aren't doing PAP, or if we are, */
  /* that it is in open state */

  if (!lcp_gotoptions[cstate->unit].neg_chap ||
      (lcp_gotoptions[cstate->unit].neg_chap &&
       upap[cstate->unit].us_serverstate == UPAPCS_OPEN ))
    ipcp_activeopen(cstate->unit);	/* Start IPCP */
}


/*
 * ChapReceiveFailure - Receive failure.
 */
/* ARGSUSED */
static void
  ChapReceiveFailure(cstate, inp, id, len)
chap_state *cstate;
u_char *inp;
u_char id;
int len;
{
  u_char msglen;
  u_char *msg;
  
  CHAPDEBUG((LOG_INFO, "ChapReceiveFailure: Rcvd id %d.", id))
  if (cstate->clientstate != CHAPCS_CHALLENGE_SENT) /* XXX */
    return;
  
  /*
   * Parse message.
   */
  if (len < sizeof (u_char)) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveFailure: rcvd short packet."))
    return;
  }
  GETCHAR(msglen, inp);
  len -= sizeof (u_char);
  if (len < msglen) {
    CHAPDEBUG((LOG_INFO, "ChapReceiveFailure: rcvd short packet."))
    return;
  }
  msg = inp;
  PRINTMSG(msg, msglen);
  
  cstate->flags &= ~CHAPF_UPVALID;	/* Clear valid flag */
  cstate->clientstate = CHAPCS_CLOSED;	/* Pretend for a moment */
  ChapAuthWithPeer(cstate->unit);	/* Restart */
}


/*
 * ChapSendChallenge - Send an Authenticate challenge.
 */
static void
  ChapSendChallenge(cstate)
chap_state *cstate;
{
  u_char *outp;
  u_char chal_len;
  int outlen;

/* pick a random challenge length between MIN_CHALLENGE_LENGTH and 
   MAX_CHALLENGE_LENGTH */  
  cstate->chal_len =  (unsigned) ((drand48() *
			   (MAX_CHALLENGE_LENGTH - MIN_CHALLENGE_LENGTH)) +
			  MIN_CHALLENGE_LENGTH);
  chal_len = cstate->chal_len;

  outlen = CHAP_HEADERLEN + 2 * sizeof (u_char) + chal_len + hostname_len;
  outp = outpacket_buf;

  MAKEHEADER(outp, CHAP);		/* paste in a CHAP header */
  
  PUTCHAR(CHAP_CHALLENGE, outp);
  PUTCHAR(++cstate->id, outp);
  PUTSHORT(outlen, outp);

  PUTCHAR(chal_len, outp);	/* put length of challenge */

  ChapGenChallenge(chal_len, cstate->chal_str); /* generate a challenge string */

  BCOPY(cstate->chal_str, outp, chal_len); /* copy it the the output buffer */
  INCPTR(chal_len, outp);

  BCOPY(hostname, outp, hostname_len); /* append hostname */
  INCPTR(hostname_len, outp);

  output(cstate->unit, outpacket_buf, outlen + DLLHEADERLEN);
  
  CHAPDEBUG((LOG_INFO, "ChapSendChallenge: Sent id %d.", cstate->id))
  cstate->clientstate |= CHAPCS_CHALLENGE_SENT;
}


/*
 * ChapSendStatus - Send a status response (ack or nak).
 */
static void
  ChapSendStatus(cstate, code, id, msg, msglen)
chap_state *cstate;
u_char code, id;
u_char *msg;
int msglen;
{
  u_char *outp;
  int outlen;
  
  outlen = CHAP_HEADERLEN + msglen;
  outp = outpacket_buf;

  MAKEHEADER(outp, CHAP);	/* paste in a header */
  
  PUTCHAR(code, outp);
  PUTCHAR(id, outp);
  PUTSHORT(outlen, outp);
  BCOPY(msg, outp, msglen);
  output(cstate->unit, outpacket_buf, outlen + DLLHEADERLEN);
  
  CHAPDEBUG((LOG_INFO, "ChapSendStatus: Sent code %d, id %d.", code, id))
}

/*
 * ChapGenChallenge is used to generate a pseudo-random challenge string of
 * a pseudo-random length between min_len and max_len and return the
 * challenge string, and the message digest of the secret appended to
 * the challenge string.  the message digest type is specified by mdtype.
 *
 * It returns with the string in the caller-supplied buffer str (which
 * should be instantiated with a length of max_len + 1), and the
 * length of the generated string into chal_len.
 *
 */

static void
  ChapGenChallenge(chal_len, str)
u_char chal_len;
u_char * str;
{
  u_char * ptr = str;
  unsigned int i;

  /* generate a random string */

  for (i = 0; i < chal_len; i++ )
    *ptr++ = (char) (drand48() * 0xff);
      
  *ptr = 0;		/* null terminate it so we can printf it */
}
/*
 * ChapSendResponse - send a response packet with the message
 *                      digest specified by md and md_len
 */
/* ARGSUSED */
static void
  ChapSendResponse(cstate, id, md, md_len)
chap_state *cstate;
u_char id;
u_char *md;
int md_len;
{
    u_char *outp;
    int outlen;

    outlen = CHAP_HEADERLEN + sizeof (u_char) + md_len + hostname_len;
    outp = outpacket_buf;
    MAKEHEADER(outp, CHAP);

    PUTCHAR(CHAP_RESPONSE, outp); /* we are a response */
    PUTCHAR(id, outp);		/* copy id from challenge packet */
    PUTSHORT(outlen, outp);	/* packet length */

    PUTCHAR(md_len, outp);	/* length of MD */

    BCOPY(md, outp, md_len);	/* copy MD to buffer */
    INCPTR(md_len, outp);

    BCOPY(hostname, outp, hostname_len); /* append hostname */
    INCPTR(hostname_len, outp);

    output(cstate->unit, outpacket_buf, outlen + DLLHEADERLEN); /* bomb's away! */
}

#ifdef NO_DRAND48

double drand48()
{
  return (double)random() / (double)0x7fffffffL; /* 2**31-1 */
}

void srand48(seedval)
long seedval;
{
  srand((int)seedval);
}

#endif
