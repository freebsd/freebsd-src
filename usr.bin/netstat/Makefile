#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD$

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mcast.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c
CFLAGS+=-DIPSEC
.if !defined(NO_INET6)
CFLAGS+=-DINET6
.endif
WARNS?=	2
NO_WERROR=

BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBNETGRAPH} ${LIBUTIL}
LDADD=	-lkvm -lipx -lnetgraph -lutil

.include <bsd.prog.mk>
