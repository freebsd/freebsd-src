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
 * $Id: hdlc.h,v 1.14.2.12 1998/05/01 19:24:39 brian Exp $
 *
 *	TODO:
 */

/*
 *  Definition for Async HDLC
 */
#define HDLC_SYN 0x7e		/* SYNC character */
#define HDLC_ESC 0x7d		/* Escape character */
#define HDLC_XOR 0x20		/* Modifier value */

#define	HDLC_ADDR 0xff
#define	HDLC_UI	  0x03
/*
 *  Definition for HDLC Frame Check Sequence
 */
#define INITFCS 0xffff		/* Initial value for FCS computation */
#define GOODFCS 0xf0b8		/* Good FCS value */

#define	DEF_MRU		1500
#define	MAX_MRU		2048
#define	MIN_MRU		296

#define	DEF_MTU		0	/* whatever peer says */
#define	MAX_MTU		2048
#define	MIN_MTU		296

/*
 *  Output priority
 */
/* PRI_NORMAL and PRI_FAST have meaning only on the IP queue.
 * All IP frames have the same priority once they are compressed.
 * IP frames stay on the IP queue till they can be sent on the
 * link. They are compressed at that time.
*/
#define	PRI_NORMAL	0	/* Normal priority */
#define	PRI_FAST	1	/* Fast (interractive) */
#define	PRI_LINK	1	/* Urgent (LQR packets) */
#define	PRI_MAX		1

struct physical;
struct link;
struct lcp;
struct bundle;
struct mbuf;
struct cmdargs;

struct hdlc {
  struct pppTimer ReportTimer;

  struct {
    int badfcs;
    int badaddr;
    int badcommand;
    int unknownproto;
  } laststats, stats;

  struct {
    struct lcp *owner;			/* parent LCP */
    struct pppTimer timer;		/* When to send */
    int method;				/* bit-mask for LQM_* from lqr.h */

    u_int32_t OutPackets;		/* Packets sent by me */
    u_int32_t OutOctets;		/* Octets sent by me */
    u_int32_t SaveInPackets;		/* Packets received from peer */
    u_int32_t SaveInDiscards;		/* Discards */
    u_int32_t SaveInErrors;		/* Errors */
    u_int32_t SaveInOctets;		/* Octets received from peer */

    struct {
      u_int32_t OutLQRs;		/* LQRs sent by me */
      u_int32_t SaveInLQRs;		/* LQRs received from peer */
      struct lqrdata peer;		/* Last LQR from peer */
      int peer_timeout;			/* peers max lqr timeout */
      int resent;			/* Resent last packet `resent' times */
    } lqr;

    struct {
      u_int32_t seq_sent;		/* last echo sent */
      u_int32_t seq_recv;		/* last echo received */
    } echo;
  } lqm;
};


extern void hdlc_Init(struct hdlc *, struct lcp *);
extern void hdlc_StartTimer(struct hdlc *);
extern void hdlc_StopTimer(struct hdlc *);
extern int hdlc_ReportStatus(struct cmdargs const *);
extern const char *hdlc_Protocol2Nam(u_short);
extern void hdlc_DecodePacket(struct bundle *, u_short, struct mbuf *,
                              struct link *);

extern void hdlc_Input(struct bundle *, struct mbuf *, struct physical *);
extern void hdlc_Output(struct link *, int, u_short, struct mbuf *bp);
extern u_short hdlc_Fcs(u_short, u_char *, int);
extern u_char *hdlc_Detect(struct physical *, u_char *, int);
