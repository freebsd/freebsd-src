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
 * $Id: ccp.h,v 1.14.2.20 1998/05/01 19:24:00 brian Exp $
 *
 *	TODO:
 */

#define	CCP_MAXCODE	CODE_RESETACK

#define	TY_OUI		0	/* OUI */
#define	TY_PRED1	1	/* Predictor type 1 */
#define	TY_PRED2	2	/* Predictor type 2 */
#define	TY_PUDDLE	3	/* Puddle Jumper */
#define	TY_HWPPC	16	/* Hewlett-Packard PPC */
#define	TY_STAC		17	/* Stac Electronics LZS */
#define	TY_MSPPC	18	/* Microsoft PPC */
#define	TY_GAND		19	/* Gandalf FZA */
#define	TY_V42BIS	20	/* V.42bis compression */
#define	TY_BSD		21	/* BSD LZW Compress */
#define	TY_PPPD_DEFLATE	24	/* Deflate (gzip) - (mis) numbered by pppd */
#define	TY_DEFLATE	26	/* Deflate (gzip) - rfc 1979 */

#define CCP_NEG_DEFLATE		0
#define CCP_NEG_PRED1		1
#define CCP_NEG_DEFLATE24	2
#define CCP_NEG_TOTAL		3

struct mbuf;
struct link;

struct ccp_config {
  struct {
    struct {
      int winsize;
    } in, out;
  } deflate;
  u_int fsmretry;		/* FSM retry frequency */
  unsigned neg[CCP_NEG_TOTAL];
};

struct ccp_opt {
  struct ccp_opt *next;
  int algorithm;
  struct lcp_opt val;
};

struct ccp {
  struct fsm fsm;		/* The finite state machine */

  int his_proto;		/* peer's compression protocol */
  int my_proto;			/* our compression protocol */

  int reset_sent;		/* If != -1, ignore compressed 'till ack */
  int last_reset;		/* We can receive more (dups) w/ this id */

  struct {
    int algorithm;		/* Algorithm in use */
    void *state;		/* Returned by implementations Init() */
    struct lcp_opt opt;		/* Set by implementations OptInit() */
  } in;

  struct {
    int algorithm;		/* Algorithm in use */
    void *state;		/* Returned by implementations Init() */
    struct ccp_opt *opt;	/* Set by implementations OptInit() */
  } out;

  u_int32_t his_reject;		/* Request codes rejected by peer */
  u_int32_t my_reject;		/* Request codes I have rejected */

  u_long uncompout, compout;
  u_long uncompin, compin;

  struct ccp_config cfg;
};

#define fsm2ccp(fp) (fp->proto == PROTO_CCP ? (struct ccp *)fp : NULL)

struct ccp_algorithm {
  int id;
  int Neg;			/* ccp_config neg array item */
  const char *(*Disp)(struct lcp_opt *);
  struct {
    int (*Set)(struct lcp_opt *, const struct ccp_config *);
    void *(*Init)(struct lcp_opt *);
    void (*Term)(void *);
    void (*Reset)(void *);
    struct mbuf *(*Read)(void *, struct ccp *, u_short *, struct mbuf *);
    void (*DictSetup)(void *, struct ccp *, u_short, struct mbuf *);
  } i;
  struct {
    void (*OptInit)(struct lcp_opt *, const struct ccp_config *);
    int (*Set)(struct lcp_opt *);
    void *(*Init)(struct lcp_opt *);
    void (*Term)(void *);
    void (*Reset)(void *);
    int (*Write)(void *, struct ccp *, struct link *, int, u_short,
                 struct mbuf *);
  } o;
};

extern void ccp_Init(struct ccp *, struct bundle *, struct link *,
                     const struct fsm_parent *);
extern void ccp_Setup(struct ccp *);

extern void ccp_SendResetReq(struct fsm *);
extern void ccp_Input(struct ccp *, struct bundle *, struct mbuf *);
extern int ccp_ReportStatus(struct cmdargs const *);
extern int ccp_Compress(struct ccp *, struct link *, int, u_short, struct mbuf *);
extern struct mbuf *ccp_Decompress(struct ccp *, u_short *, struct mbuf *);
extern u_short ccp_Proto(struct ccp *);
extern void ccp_SetupCallbacks(struct ccp *);
