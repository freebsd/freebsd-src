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
 * $Id: hdlc.h,v 1.10 1997/08/25 00:29:13 brian Exp $
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

extern u_char EscMap[33];

extern void HdlcInit(void);
extern void HdlcErrorCheck(void);
extern void HdlcInput(struct mbuf *);
extern void HdlcOutput(int, u_short, struct mbuf *bp);
extern void AsyncOutput(int, struct mbuf *, int);
extern u_short HdlcFcs(u_short, u_char *, int);
extern void DecodePacket(u_short, struct mbuf *);
extern int ReportHdlcStatus(void);
extern int ReportProtStatus(void);
