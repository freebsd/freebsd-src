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
 * $Id: lcp.h,v 1.13 1997/11/22 03:37:36 brian Exp $
 *
 *	TODO:
 */

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

struct lcpstate {
  u_long his_mru;
  u_long his_accmap;
  u_long his_magic;
  u_long his_lqrperiod;
  u_char his_protocomp;
  u_char his_acfcomp;
  u_short his_auth;

  u_long want_mru;
  u_long want_accmap;
  u_long want_magic;
  u_long want_lqrperiod;
  u_char want_protocomp;
  u_char want_acfcomp;
  u_short want_auth;

  u_long his_reject;		/* Request codes rejected by peer */
  u_long my_reject;		/* Request codes I have rejected */

  u_short auth_iwait;
  u_short auth_ineed;
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


extern struct lcpstate LcpInfo;
extern struct fsm LcpFsm;

extern void LcpInit(void);
extern void LcpUp(void);
extern void LcpSendProtoRej(u_char *, int);
extern void LcpOpen(int);
extern void LcpClose(void);
extern void LcpDown(void);
extern int LcpPutConf(int, u_char *, const struct lcp_opt *, const char *,
                       const char *, ...);
extern int ReportLcpStatus(struct cmdargs const *);
extern void LcpInput(struct mbuf *);
