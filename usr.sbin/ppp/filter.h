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
 * $Id: filter.h,v 1.4.2.3 1997/08/25 00:34:25 brian Exp $
 *
 *	TODO:
 */

/*
 *   Actions
 */
#define	A_NONE		0
#define	A_PERMIT	1
#define	A_DENY		2
#define	A_MASK		3
#define	A_UHOST		4
#define	A_UPORT		8

/*
 *   Known protocols
 */
#define	P_NONE	0
#define	P_TCP	1
#define	P_UDP	2
#define	P_ICMP	3

/*
 *   Operations
 */
#define	OP_NONE	0
#define	OP_EQ	1
#define	OP_GT	2
#define	OP_LT	4

struct filterent {
  int action;			/* Filtering action */
  int swidth;			/* Effective source address width */
  struct in_addr saddr;		/* Source address */
  struct in_addr smask;		/* Source address mask */
  int dwidth;			/* Effective destination address width */
  struct in_addr daddr;		/* Destination address */
  struct in_addr dmask;		/* Destination address mask */
  int proto;			/* Protocol */
  struct {
    short srcop;
    u_short srcport;
    short dstop;
    u_short dstport;
    int estab;
  } opt;
};

#define	MAXFILTERS	20

#define FL_IN		0
#define FL_OUT		1
#define FL_DIAL		2
#define FL_KEEP		3

extern struct filterent ifilters[MAXFILTERS];	/* incoming packet filter */
extern struct filterent ofilters[MAXFILTERS];	/* outgoing packet filter */
extern struct filterent dfilters[MAXFILTERS];	/* dial-out packet filter */
extern struct filterent afilters[MAXFILTERS];	/* keep-alive packet filter */

extern int ParseAddr(int, char const *const *, struct in_addr *, struct in_addr *, int *);
extern int ShowIfilter(struct cmdargs const *);
extern int ShowOfilter(struct cmdargs const *);
extern int ShowDfilter(struct cmdargs const *);
extern int ShowAfilter(struct cmdargs const *);
extern int SetIfilter(struct cmdargs const *);
extern int SetOfilter(struct cmdargs const *);
extern int SetDfilter(struct cmdargs const *);
extern int SetAfilter(struct cmdargs const *);
