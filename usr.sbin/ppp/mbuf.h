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
 * $Id: mbuf.h,v 1.17 1999/05/09 20:02:25 brian Exp $
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

#define MBUF_CTOP(bp) \
	((bp) ? (u_char *)((bp)+1) + (bp)->offset : NULL)

#define CONST_MBUF_CTOP(bp) \
	((bp) ? (const u_char *)((bp)+1) + (bp)->offset : NULL)

#define MB_IPIN		0
#define MB_IPOUT	1
#define MB_ALIASIN	2
#define MB_ALIASOUT	3
#define MB_MPIN		4
#define MB_MPOUT	5
#define MB_VJIN		6
#define MB_VJOUT	7
#define MB_ICOMPDIN	8
#define MB_ICOMPDOUT	9
#define MB_COMPDIN	10
#define MB_COMPDOUT	11
#define MB_LQRIN	12
#define MB_LQROUT	13
#define MB_ECHOIN	14
#define MB_ECHOOUT	15
#define MB_PROTOIN	16
#define MB_PROTOOUT	17
#define MB_ACFIN	18
#define MB_ACFOUT	19
#define MB_SYNCIN	20
#define MB_SYNCOUT	21
#define MB_HDLCIN	22
#define MB_HDLCOUT	23
#define MB_ASYNCIN	24
#define MB_ASYNCOUT	25
#define MB_CBCPIN	26
#define MB_CBCPOUT	27
#define MB_CHAPIN	28
#define MB_CHAPOUT	29
#define MB_PAPIN	30
#define MB_PAPOUT	31
#define MB_CCPIN	32
#define MB_CCPOUT	33
#define MB_IPCPIN	34
#define MB_IPCPOUT	35
#define MB_LCPIN	36
#define MB_LCPOUT	37
#define MB_UNKNOWN	38
#define MB_MAX		MB_UNKNOWN

struct cmdargs;

extern int mbuf_Length(struct mbuf *);
extern struct mbuf *mbuf_Alloc(int, int);
extern struct mbuf *mbuf_FreeSeg(struct mbuf *);
extern void mbuf_Free(struct mbuf *);
extern void mbuf_Write(struct mbuf *, const void *, size_t);
extern struct mbuf *mbuf_Read(struct mbuf *, void *, size_t);
extern size_t mbuf_View(struct mbuf *, void *, size_t);
extern struct mbuf *mbuf_Prepend(struct mbuf *, const void *, size_t, size_t);
extern struct mbuf *mbuf_Truncate(struct mbuf *, size_t);
extern int mbuf_Show(struct cmdargs const *);
extern void mbuf_Enqueue(struct mqueue *, struct mbuf *);
extern struct mbuf *mbuf_Dequeue(struct mqueue *);
extern struct mbuf *mbuf_Contiguous(struct mbuf *);
extern void mbuf_SetType(struct mbuf *, int);
