/*-
 * Copyright (c) 1996 - 2001 Brian Somers <brian@Awfulhak.org>
 *          based on work by Toshiharu OHNO <tony-o@iij.ad.jp>
 *                           Internet Initiative Japan, Inc (IIJ)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Known protocols - f_proto */
#define	P_NONE	0
#define	P_TCP	1
#define	P_UDP	2
#define	P_ICMP	3
#ifdef IPPROTO_OSPFIGP
#define	P_OSPF	4
#endif
#define	P_IGMP	5
#ifdef IPPROTO_GRE
#define P_GRE	6
#endif
#define P_ESP	7
#define P_AH	8
#define P_IPIP	9

/* Operations - f_srcop, f_dstop */
#define	OP_NONE	0
#define	OP_EQ	1
#define	OP_GT	2
#define	OP_LT	3

/* srctype or dsttype */
#define T_ADDR		0
#define T_MYADDR	1
#define T_HISADDR	2
#define T_DNS0		3
#define T_DNS1		4

/*
 * There's a struct filterent for each possible filter rule.  The
 * layout is designed to minimise size (there are 4 * MAXFILTERS of
 * them) - which is also conveniently a power of 2 (32 bytes) on
 * architectures where sizeof(int)==4 (this makes indexing faster).
 *
 * f_action and f_proto only need to be 6 and 3 bits, respectively,
 * but making them 8 bits allows them to be efficently accessed using
 * byte operations as well as allowing space for future expansion
 * (expanding MAXFILTERS or converting f_proto IPPROTO_... values).
 *
 * Note that there are four free bits in the initial word for future
 * extensions.
 */
struct filterent {
  unsigned f_action : 8;		/* Filtering action: goto or A_... */
  unsigned f_proto : 8;		/* Protocol: P_... */
  unsigned f_srcop : 2;		/* Source port operation: OP_... */
  unsigned f_dstop : 2;		/* Destination port operation: OP_... */
  unsigned f_srctype : 3;	/* T_ value of src */
  unsigned f_dsttype : 3;	/* T_ value of dst */
  unsigned f_estab : 1;		/* Check TCP ACK bit */
  unsigned f_syn : 1;		/* Check TCP SYN bit */
  unsigned f_finrst : 1;	/* Check TCP FIN/RST bits */
  unsigned f_invert : 1;	/* true to complement match */
  struct in_range f_src;	/* Source address and mask */
  struct in_range f_dst;	/* Destination address and mask */
  u_short f_srcport;		/* Source port, compared with f_srcop */
  u_short f_dstport;		/* Destination port, compared with f_dstop */
  unsigned timeout;		/* Keep alive value for passed packet */
};

#define	MAXFILTERS	40	/* in each filter set */

/* f_action values [0..MAXFILTERS) specify the next filter rule, others are: */
#define	A_NONE		(MAXFILTERS)
#define	A_PERMIT	(A_NONE+1)
#define	A_DENY		(A_PERMIT+1)

struct filter {
  struct filterent rule[MAXFILTERS];	/* incoming packet filter */
  const char *name;
  unsigned fragok : 1;
  unsigned logok : 1;
};

/* Which filter set */
#define FL_IN		0
#define FL_OUT		1
#define FL_DIAL		2
#define FL_KEEP		3

struct ipcp;
struct cmdargs;

extern int ParseAddr(struct ipcp *, const char *, struct in_addr *,
                     struct in_addr *, int *);
extern int filter_Show(struct cmdargs const *);
extern int filter_Set(struct cmdargs const *);
extern const char * filter_Action2Nam(int);
extern const char *filter_Proto2Nam(int);
extern const char *filter_Op2Nam(int);
extern struct in_addr bits2mask(int);
extern void filter_AdjustAddr(struct filter *, struct in_addr *,
                              struct in_addr *, struct in_addr [2]);
