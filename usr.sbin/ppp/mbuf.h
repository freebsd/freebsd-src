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
 * $Id: mbuf.h,v 1.10 1997/12/03 10:23:50 brian Exp $
 *
 *	TODO:
 */

struct mbuf {
  u_char *base;			/* pointer to top of buffer space */
  short size;			/* size allocated from base */
  short offset;			/* offset to start position */
  short cnt;			/* available byte count in buffer */
  short type;
  struct mbuf *next;		/* link to next mbuf */
  struct mbuf *pnext;		/* link to next packet */
};

struct mqueue {
  struct mbuf *top;
  struct mbuf *last;
  int qlen;
};

#define MBUF_CTOP(bp)   (bp->base + bp->offset)

#define MB_ASYNC	1
#define MB_FSM		2
#define MB_HDLCOUT	3
#define MB_IPIN		4
#define MB_ECHO		5
#define MB_LQR		6
#define MB_MODEM	7
#define MB_VJCOMP	8
#define	MB_LOG		9
#define	MB_IPQ		10
#define	MB_MAX		MB_IPQ

extern int plength(struct mbuf *);
extern struct mbuf *mballoc(int, int);
extern struct mbuf *mbfree(struct mbuf *);
extern void pfree(struct mbuf *);
extern void mbwrite(struct mbuf *, u_char *, int);
extern struct mbuf *mbread(struct mbuf *, u_char *, int);
extern void LogMemory(void);
extern int ShowMemMap(struct cmdargs const *);
