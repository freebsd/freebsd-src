# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c # iso.c ns.c tp_astring.c 
CFLAGS+=-Wall
CFLAGS+=-DIPSEC
CFLAGS+=-DINET6

#CFLAGS+=-g
#.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBNETGRAPH} ${LIBUTIL}
LDADD=	-lkvm -lipx -lnetgraph -lutil

.include <bsd.prog.mk>
