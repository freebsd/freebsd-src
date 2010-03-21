#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD: src/usr.bin/netstat/Makefile,v 1.39.2.2.4.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mcast.c mroute.c route.c \
	unix.c atalk.c mroute6.c ipsec.c bpf.c pfkey.c sctp.c

WARNS?=	3
CFLAGS+=-fno-strict-aliasing

CFLAGS+=-DIPSEC
CFLAGS+=-DSCTP

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	inet6.c
CFLAGS+=-DINET6
.endif

BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=	-lkvm -lmemstat -lutil

.if ${MK_NETGRAPH_SUPPORT} != "no"
SRCS+=	netgraph.c
DPADD+=	${LIBNETGRAPH}
LDADD+=	-lnetgraph
CFLAGS+=-DNETGRAPH
.endif

.if ${MK_IPX_SUPPORT} != "no"
SRCS+=	ipx.c
DPADD+=	${LIBIPX}
LDADD+=	-lipx
CFLAGS+=-DIPX
.endif

.include <bsd.prog.mk>
