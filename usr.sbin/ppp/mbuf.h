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
 * $FreeBSD: src/usr.sbin/ppp/mbuf.h,v 1.24 2000/01/03 20:09:22 brian Exp $
 *
 *	TODO:
 */

struct mbuf {
  size_t m_size;		/* size allocated (excluding header) */
  short m_offset;		/* offset from header end to start position */
  size_t m_len;			/* available byte count in buffer */
  short m_type;			/* MB_* below */
  struct mbuf *m_next;		/* link to next mbuf */
  struct mbuf *m_nextpkt;	/* link to next packet */
  /* buffer space is malloc()d directly after the header */
};

struct mqueue {
  struct mbuf *top;
  struct mbuf *last;
  size_t len;
};

#define MBUF_CTOP(bp) \
	((bp) ? (u_char *)((bp)+1) + (bp)->m_offset : NULL)

#define CONST_MBUF_CTOP(bp) \
	((bp) ? (const u_char *)((bp)+1) + (bp)->m_offset : NULL)

#define MB_IPIN		0
#define MB_IPOUT	1
#define MB_NATIN	2
#define MB_NATOUT	3
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

#define M_MAXLEN	(4096 - sizeof(struct mbuf))

struct cmdargs;

extern int m_length(struct mbuf *);
extern struct mbuf *m_get(size_t, int);
extern struct mbuf *m_free(struct mbuf *);
extern void m_freem(struct mbuf *);
extern void mbuf_Write(struct mbuf *, const void *, size_t);
extern struct mbuf *mbuf_Read(struct mbuf *, void *, size_t);
extern size_t mbuf_View(struct mbuf *, void *, size_t);
extern struct mbuf *m_prepend(struct mbuf *, const void *, size_t, size_t);
extern struct mbuf *m_adj(struct mbuf *, ssize_t);
extern struct mbuf *m_pullup(struct mbuf *);
extern void m_settype(struct mbuf *, int);
extern struct mbuf *m_append(struct mbuf *, const void *, size_t);

extern int mbuf_Show(struct cmdargs const *);

extern void m_enqueue(struct mqueue *, struct mbuf *);
extern struct mbuf *m_dequeue(struct mqueue *);
