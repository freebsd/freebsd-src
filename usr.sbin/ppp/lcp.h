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
 * $Id: lcp.h,v 1.16.2.13 1998/03/13 00:44:45 brian Exp $
 *
 *	TODO:
 */

#define	REJECTED(p, x)	((p)->his_reject & (1<<(x)))

struct lcp {
  struct fsm fsm;		/* The finite state machine */
  u_int16_t his_mru;		/* Peers maximum packet size */
  u_int32_t his_accmap;		/* Peeers async char control map */
  u_int32_t his_magic;		/* Peers magic number */
  u_int32_t his_lqrperiod;	/* Peers LQR frequency */
  int his_protocomp : 1;	/* Does peer do Protocol field compression */
  int his_acfcomp : 1;		/* Does peer do addr & cntrl fld compression */
  u_short his_auth;		/* Peer wants this type of authentication */

  u_short want_mru;		/* Our maximum packet size */
  u_int32_t want_accmap;	/* Our async char control map */
  u_int32_t want_magic;		/* Our magic number */
  u_int32_t want_lqrperiod;	/* Our LQR frequency */
  int want_protocomp : 1;	/* Do we do protocol field compression */
  int want_acfcomp : 1;		/* Do we do addr & cntrl fld compression */
  u_short want_auth;		/* We want this type of authentication */

  u_int32_t his_reject;		/* Request codes rejected by peer */
  u_int32_t my_reject;		/* Request codes I have rejected */

  u_short auth_iwait;		/* I must authenticate to the peer */
  u_short auth_ineed;		/* I require that the peer authenticates */

  int LcpFailedMagic;		/* Number of `magic is same' errors */
};

#define	LCP_MAXCODE	CODE_DISCREQ

#define	TY_MRU		1	/* Maximum-Receive-Unit */
#define	TY_ACCMAP	2	/* Async-Control-Character-Map */
#define	TY_AUTHPROTO	3	/* Authentication-Protocol */
#define	TY_QUALPROTO	4	/* Quality-Protocol */
#define	TY_MAGICNUM	5	/* Magic-Number */
#define	TY_RESERVED	6	/* RESERVED */
#define	TY_PROTOCOMP	7	/* Protocol-Field-Compression */
#define	TY_ACFCOMP	8	/* Address-and-Control-Field-Compression */
#define	TY_FCSALT	9	/* FCS-Alternatives */
#define	TY_SDP		10	/* Self-Describing-Padding */

#define MAX_LCP_OPT_LEN 10
struct lcp_opt {
  u_char id;
  u_char len;
  u_char data[MAX_LCP_OPT_LEN-2];
};

struct physical;

#define fsm2lcp(fp) (fp->proto == PROTO_LCP ? (struct lcp *)fp : NULL)
#define lcp2ccp(lcp) (&link2physical((lcp)->fsm.link)->dl->ccp)

extern void lcp_Init(struct lcp *, struct bundle *, struct physical *,
                     const struct fsm_parent *);
extern void lcp_Setup(struct lcp *, int);

extern void lcp_SendProtoRej(struct lcp *, u_char *, int);
extern int LcpPutConf(int, u_char *, const struct lcp_opt *, const char *,
                       const char *, ...);
extern int lcp_ReportStatus(struct cmdargs const *);
extern void LcpInput(struct lcp *, struct mbuf *);
