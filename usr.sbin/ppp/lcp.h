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
 * $FreeBSD: src/usr.sbin/ppp/lcp.h,v 1.23.2.1 2000/08/19 09:30:04 brian Exp $
 *
 *	TODO:
 */

/* callback::opmask values */
#define CALLBACK_AUTH		(0)
#define CALLBACK_DIALSTRING	(1)	/* Don't do this */
#define CALLBACK_LOCATION	(2)	/* Don't do this */
#define CALLBACK_E164		(3)
#define CALLBACK_NAME		(4)	/* Don't do this */
#define CALLBACK_CBCP		(6)
#define CALLBACK_NONE		(14)	/* No callback is ok */

#define CALLBACK_BIT(n) ((n) < 0 ? 0 : 1 << (n))

struct callback {
  int opmask;			/* want these types of callback */
  char msg[SCRIPT_LEN];		/* with this data (E.164) */
};

#define	REJECTED(p, x)	((p)->his_reject & (1<<(x)))

struct lcp {
  struct fsm fsm;		/* The finite state machine */
  u_int16_t his_mru;		/* Peers maximum packet size */
  u_int16_t his_mrru;		/* Peers maximum reassembled packet size (MP) */
  u_int32_t his_accmap;		/* Peeers async char control map */
  u_int32_t his_magic;		/* Peers magic number */
  u_int32_t his_lqrperiod;	/* Peers LQR frequency (100ths of seconds) */
  u_short his_auth;		/* Peer wants this type of authentication */
  u_char his_authtype;		/* Fifth octet of REQ/NAK/REJ */
  struct callback his_callback;	/* Peer wants callback ? */
  unsigned his_shortseq : 1;	/* Peer would like only 12bit seqs (MP) */
  unsigned his_protocomp : 1;	/* Does peer do Protocol field compression */
  unsigned his_acfcomp : 1;	/* Does peer do addr & cntrl fld compression */

  u_short want_mru;		/* Our maximum packet size */
  u_short want_mrru;		/* Our maximum reassembled packet size (MP) */
  u_int32_t want_accmap;	/* Our async char control map */
  u_int32_t want_magic;		/* Our magic number */
  u_int32_t want_lqrperiod;	/* Our LQR frequency (100ths of seconds) */
  u_short want_auth;		/* We want this type of authentication */
  u_char want_authtype;		/* Fifth octet of REQ/NAK/REJ */
  struct callback want_callback;/* We want callback ? */
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
    u_int32_t lqrperiod;	/* LQR frequency (seconds) */
    struct fsm_retry fsm;	/* How often/frequently to resend requests */
    unsigned acfcomp : 2;	/* Address & Control Field Compression neg */
    unsigned chap05 : 2;	/* Challenge Handshake Authentication proto */
#ifdef HAVE_DES
    unsigned chap80nt : 2;	/* Microsoft (NT) CHAP */
    unsigned chap80lm : 2;	/* Microsoft (LANMan) CHAP */
#endif
    unsigned lqr : 2;		/* Link Quality Report */
    unsigned pap : 2;		/* Password Authentication protocol */
    unsigned protocomp : 2;	/* Protocol field compression */
    char ident[DEF_MRU - 7];	/* SendIdentification() data */
  } cfg;
};

#define	LCP_MAXCODE	CODE_IDENT
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
#define	TY_CALLBACK	13	/* Callback */
#define	TY_CFRAMES	15	/* Compound-frames */
#define	TY_MRRU		17	/* Max Reconstructed Receive Unit (MP) */
#define	TY_SHORTSEQ	18	/* Want short seqs (12bit) please (see mp.h) */
#define	TY_ENDDISC	19	/* Endpoint discriminator */

#define MAX_LCP_OPT_LEN 20
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
struct bundle;
struct cmdargs;

#define fsm2lcp(fp) (fp->proto == PROTO_LCP ? (struct lcp *)fp : NULL)

extern void lcp_Init(struct lcp *, struct bundle *, struct link *,
                     const struct fsm_parent *);
extern void lcp_Setup(struct lcp *, int);

extern void lcp_SendProtoRej(struct lcp *, u_char *, int);
extern int lcp_SendIdentification(struct lcp *);
extern void lcp_RecvIdentification(struct lcp *, char *);
extern int lcp_ReportStatus(struct cmdargs const *);
extern struct mbuf *lcp_Input(struct bundle *, struct link *, struct mbuf *);
extern void lcp_SetupCallbacks(struct lcp *);
