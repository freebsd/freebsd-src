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
 * $Id: lcp.h,v 1.16.2.23 1998/05/01 19:24:57 brian Exp $
 *
 *	TODO:
 */

#define	REJECTED(p, x)	((p)->his_reject & (1<<(x)))

struct lcp {
  struct fsm fsm;		/* The finite state machine */
  u_int16_t his_mru;		/* Peers maximum packet size */
  u_int16_t his_mrru;		/* Peers maximum reassembled packet size (MP) */
  u_int32_t his_accmap;		/* Peeers async char control map */
  u_int32_t his_magic;		/* Peers magic number */
  u_int32_t his_lqrperiod;	/* Peers LQR frequency */
  u_short his_auth;		/* Peer wants this type of authentication */
  unsigned his_shortseq : 1;	/* Peer would like only 12bit seqs (MP) */
  unsigned his_protocomp : 1;	/* Does peer do Protocol field compression */
  unsigned his_acfcomp : 1;	/* Does peer do addr & cntrl fld compression */

  u_short want_mru;		/* Our maximum packet size */
  u_short want_mrru;		/* Our maximum reassembled packet size (MP) */
  u_int32_t want_accmap;	/* Our async char control map */
  u_int32_t want_magic;		/* Our magic number */
  u_int32_t want_lqrperiod;	/* Our LQR frequency */
  u_short want_auth;		/* We want this type of authentication */
  unsigned want_shortseq : 1;	/* I'd like only 12bit seqs (MP) */
  unsigned want_protocomp : 1;	/* Do we do protocol field compression */
  unsigned want_acfcomp : 1;	/* Do we do addr & cntrl fld compression */

  u_int32_t his_reject;		/* Request codes rejected by peer */
  u_int32_t my_reject;		/* Request codes I have rejected */

  u_short auth_iwait;		/* I must authenticate to the peer */
  u_short auth_ineed;		/* I require that the peer authenticates */

  int LcpFailedMagic;		/* Number of `magic is same' errors */

  struct {
    u_short mru;		/* Preferred MRU value */
    u_int32_t accmap;		/* Initial ACCMAP value */
    int openmode;		/* when to start CFG REQs */
    u_int lqrperiod;		/* LQR frequency */
    u_int fsmretry;		/* FSM retry frequency */

    unsigned acfcomp : 2;	/* Address & Control Field Compression neg */
    unsigned chap : 2;		/* Challenge Handshake Authentication proto */
    unsigned lqr : 2;		/* Link Quality Report */
    unsigned pap : 2;		/* Password Authentication protocol */
    unsigned protocomp : 2;	/* Protocol field compression */
  } cfg;
};

#define	LCP_MAXCODE	CODE_DISCREQ
#define	LCP_MINMPCODE	CODE_CODEREJ

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
#define	TY_MRRU		17	/* Max Reconstructed Receive Unit (MP) */
#define	TY_SHORTSEQ	18	/* Want short seqs (12bit) please (see mp.h) */
#define	TY_ENDDISC	19	/* Endpoint discriminator */

#define MAX_LCP_OPT_LEN 10
struct lcp_opt {
  u_char id;
  u_char len;
  u_char data[MAX_LCP_OPT_LEN-2];
};

#define INC_LCP_OPT(ty, length, o)                    \
  do {                                                \
    (o)->id = (ty);                                   \
    (o)->len = (length);                              \
    (o) = (struct lcp_opt *)((char *)(o) + (length)); \
  } while (0)

struct mbuf;
struct link;
struct physical;
struct bundle;
struct cmdargs;

#define fsm2lcp(fp) (fp->proto == PROTO_LCP ? (struct lcp *)fp : NULL)

extern void lcp_Init(struct lcp *, struct bundle *, struct link *,
                     const struct fsm_parent *);
extern void lcp_Setup(struct lcp *, int);

extern void lcp_SendProtoRej(struct lcp *, u_char *, int);
extern int lcp_ReportStatus(struct cmdargs const *);
extern void lcp_Input(struct lcp *, struct mbuf *);
extern void lcp_SetupCallbacks(struct lcp *);
