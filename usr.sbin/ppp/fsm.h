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
 * $Id: fsm.h,v 1.4 1995/02/27 03:17:58 amurai Exp $
 *
 *	TODO:
 */

#ifndef _FSM_H_
#define	_FSM_H_

#include "defs.h"
#include <netinet/in.h>
#include "timeout.h"
#include "cdefs.h"

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

#define	OPEN_ACTIVE	0
#define	OPEN_PASSIVE	1

struct fsm {
  char	  *name;		/* Name of protocol */
  u_short proto;		/* Protocol number */
  u_short max_code;
  int	  open_mode;
  int	  state;		/* State of the machine */
  int	  reqid;		/* Next request id */
  int	  restart;		/* Restart counter value */
  int	  maxconfig;

  int     reqcode;		/* Request code sent */
  struct pppTimer FsmTimer;	/* Restart Timer */

  void	  (*LayerUp) __P((struct fsm *));
  void	  (*LayerDown) __P((struct fsm *));
  void	  (*LayerStart) __P((struct fsm *));
  void	  (*LayerFinish) __P((struct fsm *));
  void	  (*InitRestartCounter) __P((struct fsm *));
  void	  (*SendConfigReq) __P((struct fsm *));
  void	  (*SendTerminateReq) __P((struct fsm *));
  void	  (*SendTerminateAck) __P((struct fsm *));
  void	  (*DecodeConfig) __P((u_char *, int, int));
};

struct fsmheader {
  u_char  code;		/* Request code */
  u_char  id;		/* Identification */
  u_short length;	/* Length of packet */
};

#define	CODE_CONFIGREQ	1
#define	CODE_CONFIGACK	2
#define	CODE_CONFIGNAK	3
#define	CODE_CONFIGREJ	4
#define	CODE_TERMREQ	5
#define	CODE_TERMACK	6
#define	CODE_CODEREJ	7
#define	CODE_PROTOREJ	8
#define	CODE_ECHOREQ	9		/* Used in LCP */
#define	CODE_ECHOREP	10		/* Used in LCP */
#define	CODE_DISCREQ	11
#define	CODE_IDENT	12		/* Used in LCP Extension */
#define	CODE_TIMEREM	13		/* Used in LCP Extension */
#define	CODE_RESETREQ	14		/* Used in CCP */
#define	CODE_RESETACK	15		/* Used in CCP */

struct fsmcodedesc {
  void (*action) __P((struct fsm *, struct fsmheader *, struct mbuf *));
  char *name;
};

struct fsmconfig {
  u_char type;
  u_char length;
};

u_char AckBuff[200];
u_char NakBuff[200];
u_char RejBuff[100];
u_char ReqBuff[200];

u_char *ackp, *nakp, *rejp;

extern char const *StateNames[];
extern void FsmInit __P((struct fsm *));
extern void NewState __P((struct fsm *, int));
extern void FsmOutput __P((struct fsm *, u_int, u_int, u_char *, int));
extern void FsmOpen __P((struct fsm *));
extern void FsmUp __P((struct fsm *));
extern void FsmDown __P((struct fsm *));
extern void FsmInput __P((struct fsm *, struct mbuf *));

extern void FsmRecvConfigReq __P((struct fsm *, struct fsmheader *, struct mbuf *));
extern void FsmRecvConfigAck __P((struct fsm *, struct fsmheader *, struct mbuf *));
extern void FsmRecvConfigNak __P((struct fsm *, struct fsmheader *, struct mbuf *));
extern void FsmRecvTermReq __P((struct fsm *, struct fsmheader *, struct mbuf *));
extern void FsmRecvTermAck __P((struct fsm *, struct fsmheader *, struct mbuf *));
extern void FsmClose __P((struct fsm *fp));

extern struct fsm LcpFsm, IpcpFsm, CcpFsm;

#endif	/* _FSM_H_ */
