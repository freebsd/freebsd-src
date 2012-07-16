#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mroute.c netisr.c route.c \
	unix.c atalk.c mroute6.c ipsec.c bpf.c pfkey.c sctp.c

WARNS?=	3
CFLAGS+=-fno-strict-aliasing

CFLAGS+=-DIPSEC
CFLAGS+=-DSCTP

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+=-DINET
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	inet6.c
CFLAGS+=-DINET6
.endif

.if ${MK_OFED} != "no"
CFLAGS+=-DSDP
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
