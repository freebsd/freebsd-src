/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: fsm.h,v 1.16.2.5 1998/02/06 02:24:16 brian Exp $
 *
 *	TODO:
 */

/*
 *  State of machine
 */
#define	ST_INITIAL	0
#define	ST_STARTING	1
#define	ST_CLOSED	2
#define	ST_STOPPED	3
#define	ST_CLOSING	4
#define	ST_STOPPING	5
#define	ST_REQSENT	6
#define	ST_ACKRCVD	7
#define	ST_ACKSENT	8
#define	ST_OPENED	9

#define	ST_MAX		10
#define	ST_UNDEF	-1

#define	MODE_REQ	0
#define	MODE_NAK	1
#define	MODE_REJ	2
#define	MODE_NOP	3
#define	MODE_ACK	4	/* pseudo mode for ccp negotiations */

#define	OPEN_PASSIVE	-1

struct fsm;

struct fsm_callbacks {
  void (*LayerUp) (struct fsm *);            /* Layer is now up (tlu) */
  void (*LayerDown) (struct fsm *);          /* About to come down (tld) */
  void (*LayerStart) (struct fsm *);         /* Layer about to start up (tls) */
  void (*LayerFinish) (struct fsm *);        /* Layer now down (tlf) */
  void (*InitRestartCounter) (struct fsm *); /* Set fsm timer load */
  void (*SendConfigReq) (struct fsm *);      /* Send REQ please */
  void (*SendTerminateReq) (struct fsm *);   /* Term REQ just sent */
  void (*SendTerminateAck) (struct fsm *);   /* Send Term ACK please */
  void (*DecodeConfig) (struct fsm *, u_char *, int, int);
};

struct fsm {
  const char *name;		/* Name of protocol */
  u_short proto;		/* Protocol number */
  u_short max_code;
  int open_mode;		/* Delay before config REQ (-1 forever) */
  int state;			/* State of the machine */
  u_char reqid;			/* Next request id */
  int restart;			/* Restart counter value */
  int maxconfig;		/* Max config REQ (overridden in Init funcs) */

  struct pppTimer FsmTimer;	/* Restart Timer */
  struct pppTimer OpenTimer;	/* Delay before opening */

  /*
   * This timer times the ST_STOPPED state out after the given value
   * (specified via "set stopped ...").  Although this isn't specified in the
   * rfc, the rfc *does* say that "the application may use higher level
   * timers to avoid deadlock". The StoppedTimer takes effect when the other
   * side ABENDs rather than going into ST_ACKSENT (and sending the ACK),
   * causing ppp to time out and drop into ST_STOPPED.  At this point,
   * nothing will change this state :-(
   */
  struct pppTimer StoppedTimer;
  int LogLevel;

  /* The link layer active with this FSM (may be our bundle below) */
  struct link *link;

  /* Our high-level link */
  struct bundle *bundle;

  const struct fsm_callbacks *fn;
};

struct fsmheader {
  u_char code;			/* Request code */
  u_char id;			/* Identification */
  u_short length;		/* Length of packet */
};

#define	CODE_CONFIGREQ	1
#define	CODE_CONFIGACK	2
#define	CODE_CONFIGNAK	3
#define	CODE_CONFIGREJ	4
#define	CODE_TERMREQ	5
#define	CODE_TERMACK	6
#define	CODE_CODEREJ	7
#define	CODE_PROTOREJ	8
#define	CODE_ECHOREQ	9	/* Used in LCP */
#define	CODE_ECHOREP	10	/* Used in LCP */
#define	CODE_DISCREQ	11
#define	CODE_IDENT	12	/* Used in LCP Extension */
#define	CODE_TIMEREM	13	/* Used in LCP Extension */
#define	CODE_RESETREQ	14	/* Used in CCP */
#define	CODE_RESETACK	15	/* Used in CCP */

/* Minimum config req size.  This struct is *only* used for it's size */
struct fsmconfig {
  u_char type;
  u_char length;
};

extern u_char AckBuff[200];
extern u_char NakBuff[200];
extern u_char RejBuff[100];
extern u_char ReqBuff[200];
extern u_char *ackp;
extern u_char *nakp;
extern u_char *rejp;

extern char const *StateNames[];

extern void FsmInit(struct fsm *, struct bundle *, struct link *, int);
extern void FsmOutput(struct fsm *, u_int, u_int, u_char *, int);
extern void FsmOpen(struct fsm *);
extern void FsmUp(struct fsm *);
extern void FsmDown(struct fsm *);
extern void FsmInput(struct fsm *, struct mbuf *);
extern void FsmClose(struct fsm *);
