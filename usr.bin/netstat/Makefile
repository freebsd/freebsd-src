# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c # iso.c ns.c tp_astring.c
CFLAGS+=-Wall

#CFLAGS+=-g
#.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBNETGRAPH}
LDADD=	-lkvm -lipx -lnetgraph

.include <bsd.prog.mk>
