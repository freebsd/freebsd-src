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
 * $Id: mbuf.h,v 1.15 1999/03/29 08:21:28 brian Exp $
 *
 *	TODO:
 */

struct mbuf {
  short size;			/* size allocated (excluding header) */
  short offset;			/* offset from header end to start position */
  short cnt;			/* available byte count in buffer */
  short type;			/* MB_* below */
  struct mbuf *next;		/* link to next mbuf */
  struct mbuf *pnext;		/* link to next packet */
  /* buffer space is malloc()d directly after the header */
};

struct mqueue {
  struct mbuf *top;
  struct mbuf *last;
  int qlen;
};

#define MBUF_CTOP(bp)		((u_char *)((bp)+1) + (bp)->offset)
#define CONST_MBUF_CTOP(bp)	((const u_char *)((bp)+1) + (bp)->offset)

#define MB_ASYNC	1
#define MB_FSM		2
#define MB_CBCP		3
#define MB_HDLCOUT	4
#define MB_IPIN		5
#define MB_ECHO		6
#define MB_LQR		7
#define MB_LINK		8
#define MB_VJCOMP	9
#define	MB_IPQ		10
#define	MB_MP		11
#define	MB_MAX		MB_MP

struct cmdargs;

extern int mbuf_Length(struct mbuf *);
extern struct mbuf *mbuf_Alloc(int, int);
extern struct mbuf *mbuf_FreeSeg(struct mbuf *);
extern void mbuf_Free(struct mbuf *);
extern void mbuf_Write(struct mbuf *, u_char *, int);
extern struct mbuf *mbuf_Read(struct mbuf *, u_char *, int);
extern void mbuf_Log(void);
extern int mbuf_Show(struct cmdargs const *);
extern void mbuf_Enqueue(struct mqueue *, struct mbuf *);
extern struct mbuf *mbuf_Dequeue(struct mqueue *);
extern struct mbuf *mbuf_Contiguous(struct mbuf *);
