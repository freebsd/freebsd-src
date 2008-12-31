#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD: src/usr.bin/netstat/Makefile,v 1.39.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mcast.c mroute.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c bpf.c pfkey.c sctp.c

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
DPADD=	${LIBKVM} ${LIBMEMSTAT} ${LIBNETGRAPH} ${LIBUTIL}
LDADD=	-lkvm -lmemstat -lnetgraph -lutil

.if ${MK_IPX_SUPPORT} != "no"
SRCS+=	ipx.c
DPADD+=	${LIBIPX}
LDADD+=	-lipx
CFLAGS+=-DIPX
.endif

.include <bsd.prog.mk>
